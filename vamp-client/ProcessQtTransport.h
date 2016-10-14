
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
    ProcessQtTransport(QString processName) :
        m_completenessChecker(0) {
        m_process = new QProcess();
        m_process->setReadChannel(QProcess::StandardOutput);
        m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_process->start(processName);
        if (!m_process->waitForStarted()) {
            std::cerr << "server failed to start" << std::endl;
            delete m_process;
            m_process = nullptr;
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
    setCompletenessChecker(MessageCompletenessChecker *checker) {
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

            m_process->waitForReadyRead(1000);
            qint64 byteCount = m_process->bytesAvailable();

            if (!byteCount) {
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
