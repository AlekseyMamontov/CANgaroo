/*
  Copyright (c) 2026 Jayachandran Dharuman
  This file is part of CANgaroo.
*/

#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QTimer>
#include "core/BusMessage.h"
#include "db/model/CanDbSignal.h"

class Backend;
class CanDbMessage;

enum class ConditionOperator {
    Greater,
    Less,
    Equal,
    GreaterEqual,
    LessEqual,
    NotEqual
};

struct LoggingCondition {
    CanDbSignal *signal;
    ConditionOperator op;
    double threshold;
};

class ConditionalLoggingManager : public QObject
{
    Q_OBJECT

public:
    explicit ConditionalLoggingManager(Backend &backend, QObject *parent = nullptr);
    ~ConditionalLoggingManager();

    void setConditions(const QList<LoggingCondition> &conditions, bool useAndLogic);
    void setLogSignals(const QList<CanDbSignal*> &signalList);
    const QList<CanDbSignal*>& getLogSignals() const { return _logSignals; }
    void setSignalInterfaces(const QMap<CanDbSignal*, BusInterfaceIdList> &interfaces) { _signalInterfaces = interfaces; }
    QMap<CanDbSignal*, BusInterfaceIdList> getSignalInterfaces() const { return _signalInterfaces; }
    void setLogFilePath(const QString &path);

    const QList<LoggingCondition>& getConditions() const { return _conditions; }
    bool useAndLogic() const { return _useAndLogic; }
    QString getLogFilePath() const { return _logFilePath; }

    bool isConditionMet() const { return _conditionMet; }
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }
    bool isFileLoggingEnabled() const { return _fileLoggingEnabled; }
    void setFileLoggingEnabled(bool enabled);
    void reset();

signals:
    void conditionChanged(bool met);
    void liveValuesUpdated(const QMap<CanDbSignal*, double>& values, bool isStale);

public slots:
    void processMessage(const BusMessage &msg);

private:
    void evaluate();
    void writeHeader();
    void writeDataRow(double timestamp, const QMap<CanDbSignal*, double> &values);
    
    QTimer* _timeoutTimer = nullptr;
    
private slots:
    void onTimeoutCheck();

private:
    Backend &_backend;
    bool _enabled;
    bool _conditionMet;
    bool _useAndLogic;
    bool _fileLoggingEnabled;
    QList<LoggingCondition> _conditions;
    QList<CanDbSignal*> _logSignals;
    QList<QPair<double, QMap<CanDbSignal*, double>>> _preBuffer;
    QMap<CanDbSignal*, BusInterfaceIdList> _signalInterfaces;
    QMap<CanDbSignal*, double> _signalValues;
    QMap<CanDbSignal*, qint64> _signalUpdateTimes;

    QString _logFilePath;
    QFile _logFile;
    QTextStream *_textStream;
};
