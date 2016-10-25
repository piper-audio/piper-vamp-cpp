/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
 
#ifndef PIPER_PROCESS_QT_TRANSPORT_H
#define PIPER_PROCESS_QT_TRANSPORT_H

#include "SynchronousTransport.h"

#include <QProcess>
#include <QString>
#include <QMutex>

#include <iostream>

namespace piper_vamp {
namespace client {

/**
 * A SynchronousTransport implementation that spawns a sub-process
 * using Qt's QProcess abstraction and talks to it via stdin/stdout
 * channels. Calls are completely serialized; the protocol only
 * supports one call in process at a time, and therefore the transport
 * only allows one at a time. This class is thread-safe because it
 * serializes explicitly using a mutex.
 */
class ProcessQtTransport : public SynchronousTransport
{
public:
    ProcessQtTransport(std::string processName) :
        m_completenessChecker(0) {
        m_process = new QProcess();
        m_process->setReadChannel(QProcess::StandardOutput);
        m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        QString name(QString::fromStdString(processName));

        // The second argument here is vital, otherwise we get a
        // different start() overload which parses all command args
        // out of its first argument only and therefore fails when
        // name has a space in it. This is such a gotcha that Qt5.6
        // even introduced a QT_NO_PROCESS_COMBINED_ARGUMENT_START
        // build flag to disable that overload. Unfortunately I only
        // discovered that after wasting almost a day on it.
        m_process->start(name, QStringList());
        
        if (!m_process->waitForStarted()) {
            if (m_process->state() == QProcess::NotRunning) {
                QProcess::ProcessError err = m_process->error();
                if (err == QProcess::FailedToStart) {
                    std::cerr << "Unable to start server process "
                              << processName << std::endl;
                } else if (err == QProcess::Crashed) {
                    std::cerr << "Server process " << processName
                              << " crashed on startup" << std::endl;
                } else {
                    std::cerr << "Server process " << processName
                              << " failed on startup with error code "
                              << err << std::endl;
                }
                delete m_process;
                m_process = nullptr;
            }
        }
    }

    ~ProcessQtTransport() {
        if (m_process) {
            if (m_process->state() != QProcess::NotRunning) {
		m_process->closeWriteChannel();
                m_process->waitForFinished(200);
                m_process->close();
                m_process->waitForFinished();
                std::cerr << "server exited" << std::endl;
            }
            delete m_process;
        }
    }

    void
    setCompletenessChecker(MessageCompletenessChecker *checker) override {
        //!!! ownership?
        m_completenessChecker = checker;
    }
    
    bool
    isOK() const override {
        return m_process != nullptr;
    }
    
    std::vector<char>
    call(const char *ptr, size_t size) override {

	QMutexLocker locker(&m_mutex);
	
        if (!m_completenessChecker) {
            throw std::logic_error("No completeness checker set on transport");
        }
        
        m_process->write(ptr, size);
        
        std::vector<char> buffer;
        bool complete = false;
        
        while (!complete) {

            qint64 byteCount = m_process->bytesAvailable();

	    if (!byteCount) {
		std::cerr << "waiting for data from server..." << std::endl;
		m_process->waitForReadyRead(1000);
                if (m_process->state() == QProcess::NotRunning) {
                    std::cerr << "ERROR: Subprocess exited: Load failed" << std::endl;
                    throw std::runtime_error("Piper server exited unexpectedly");
                }
            } else {
                size_t formerSize = buffer.size();
                buffer.resize(formerSize + byteCount);
                m_process->read(buffer.data() + formerSize, byteCount);
                complete = m_completenessChecker->isComplete(buffer);
            }
        }

        return buffer;
    }
    
private:
    MessageCompletenessChecker *m_completenessChecker; //!!! I don't own this (currently)
    QProcess *m_process; // I own this
    QMutex m_mutex;
};

}
}

#endif
