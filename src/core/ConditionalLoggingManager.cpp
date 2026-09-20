/*
  Copyright (c) 2026 Jayachandran Dharuman
  This file is part of CANgaroo.
*/

#include "ConditionalLoggingManager.h"
#include "core/Backend.h"
#include "core/BusMessage.h"
#include "db/model/CanDbMessage.h"
#include "db/model/CanDbSignal.h"
#include <QDateTime>
#include <QTimer>

ConditionalLoggingManager::ConditionalLoggingManager(Backend &backend, QObject *parent)
    : QObject(parent), _backend(backend), _enabled(false), _conditionMet(false), _useAndLogic(true), _fileLoggingEnabled(false), _textStream(nullptr)
{
    _timeoutTimer = new QTimer(this);
    connect(_timeoutTimer, &QTimer::timeout, this, &ConditionalLoggingManager::onTimeoutCheck);
    _timeoutTimer->start(1000); // Poll 1s frequency
}

ConditionalLoggingManager::~ConditionalLoggingManager()
{
    setEnabled(false);
}

void ConditionalLoggingManager::setEnabled(bool enabled)
{
    if (_enabled == enabled) return;
    _enabled = enabled;

    if (!_enabled) {
        if (_logFile.isOpen()) {
            _logFile.close();
        }
        delete _textStream;
        _textStream = nullptr;
        _conditionMet = false;
        _preBuffer.clear();
        emit conditionChanged(false);
    } else {
        evaluate();
    }
}

void ConditionalLoggingManager::setFileLoggingEnabled(bool enabled)
{
    if (_fileLoggingEnabled == enabled) return;
    _fileLoggingEnabled = enabled;

    if (!_fileLoggingEnabled) {
        if (_logFile.isOpen()) {
            _logFile.close();
        }
        delete _textStream;
        _textStream = nullptr;
        _preBuffer.clear();
    }
}

void ConditionalLoggingManager::reset()
{
    setEnabled(false);
    _conditions.clear();
    _logSignals.clear();
    _signalInterfaces.clear();
    _signalValues.clear();
    _logFilePath.clear();
}

void ConditionalLoggingManager::setConditions(const QList<LoggingCondition> &conditions, bool useAndLogic)
{
    _conditions = conditions;
    _useAndLogic = useAndLogic;
}

void ConditionalLoggingManager::setLogSignals(const QList<CanDbSignal*> &signalList)
{
    _logSignals = signalList;
}

void ConditionalLoggingManager::setLogFilePath(const QString &path)
{
    _logFilePath = path;
}

void ConditionalLoggingManager::processMessage(const BusMessage &msg)
{
    if (!_enabled) return;

    CanDbMessage *dbmsg = _backend.findDbMessage(msg);
    if (!dbmsg) return;

    bool relevantUpdate = false;
    BusInterfaceId msgIfId = msg.getInterfaceId();
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (CanDbSignal *signal : dbmsg->getSignals()) {
        if (signal->isPresentInMessage(msg)) {
            if (_signalInterfaces.contains(signal) && !_signalInterfaces[signal].contains(msgIfId)) {
                continue;
            }

            _signalValues[signal] = signal->extractPhysicalFromMessage(msg);
            _signalUpdateTimes[signal] = now;
            relevantUpdate = true;
        }
    }

    if (relevantUpdate) {
        if (_fileLoggingEnabled && !_conditionMet) {
            double ts = msg.getFloatTimestamp();
            _preBuffer.append(qMakePair(ts, _signalValues));

            // Prune entries older than 5 seconds
            int removeCount = 0;
            while (removeCount < _preBuffer.size() && (ts - _preBuffer[removeCount].first > 5.0)) {
                ++removeCount;
            }
            if (removeCount > 0) {
                _preBuffer.remove(0, removeCount);
            }
        }

        evaluate();

        if (_conditionMet && _textStream) {
            writeDataRow(msg.getFloatTimestamp(), _signalValues);
        }
        emit liveValuesUpdated(_signalValues, false);
    }
}

void ConditionalLoggingManager::evaluate()
{
    if (_conditions.isEmpty()) {
        if (_conditionMet) {
            _conditionMet = false;
            emit conditionChanged(false);
        }
        return;
    }

    bool result = _useAndLogic;

    for (const auto &cond : _conditions) {
        if (!_signalValues.contains(cond.signal)) {
            if (_useAndLogic) {
                result = false;
                break;
            }
            continue;
        }

        double val = _signalValues[cond.signal];
        bool condMet = false;

        switch (cond.op) {
            case ConditionOperator::Greater:      condMet = (val > cond.threshold); break;
            case ConditionOperator::Less:         condMet = (val < cond.threshold); break;
            case ConditionOperator::Equal:        condMet = (val == cond.threshold); break;
            case ConditionOperator::GreaterEqual: condMet = (val >= cond.threshold); break;
            case ConditionOperator::LessEqual:    condMet = (val <= cond.threshold); break;
            case ConditionOperator::NotEqual:     condMet = (val != cond.threshold); break;
        }

        if (_useAndLogic) {
            result &= condMet;
            if (!result) break;
        } else {
            result |= condMet;
            if (result) break;
        }
    }

    if (result != _conditionMet) {
        _conditionMet = result;
        emit conditionChanged(_conditionMet);

        if (_conditionMet) {
            // Start logging to file if path is provided
            if (_fileLoggingEnabled && !_logFilePath.isEmpty()) {
                _logFile.setFileName(_logFilePath);
                if (_logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
                    _textStream = new QTextStream(&_logFile);
                    if (_logFile.size() == 0) {
                        writeHeader();
                    }
                    
                    for (const auto &entry : _preBuffer) {
                        writeDataRow(entry.first, entry.second);
                    }
                    _preBuffer.clear();
                }
            }
        } else {
            // Stop logging to file
            if (_logFile.isOpen()) {
                _logFile.close();
            }
            delete _textStream;
            _textStream = nullptr;
        }
    }
}

void ConditionalLoggingManager::writeHeader()
{
    if (!_textStream) return;
    *_textStream << "Timestamp";
    for (CanDbSignal *sig : _logSignals) {
        *_textStream << "," << sig->name();
        if (!sig->getUnit().isEmpty()) {
            *_textStream << " [" << sig->getUnit() << "]";
        }
    }
    *_textStream << "\n";
}

void ConditionalLoggingManager::writeDataRow(double timestamp, const QMap<CanDbSignal*, double> &values)
{
    if (!_textStream) return;
    *_textStream << QString::number(timestamp, 'f', 6);
    for (CanDbSignal *sig : _logSignals) {
        *_textStream << ",";
        if (values.contains(sig)) {
            *_textStream << values[sig];
        } else {
            *_textStream << "NaN";
        }
    }
    *_textStream << "\n";
}

void ConditionalLoggingManager::onTimeoutCheck()
{
    if (!_enabled) return;
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    
    for (auto it = _signalUpdateTimes.begin(); it != _signalUpdateTimes.end();) {
        if (now - it.value() > 1500) {
            _signalValues.remove(it.key());
            it = _signalUpdateTimes.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    
    if (changed) {
        evaluate();
        emit liveValuesUpdated(_signalValues, true);
    }
}
