/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
  Piper C++

  An API for audio analysis and feature extraction plugins.

  Centre for Digital Music, Queen Mary, University of London.
  Copyright 2006-2016 Chris Cannam and QMUL.
  
  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation
  files (the "Software"), to deal in the Software without
  restriction, including without limitation the rights to use, copy,
  modify, merge, publish, distribute, sublicense, and/or sell copies
  of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  Except as contained in this notice, the names of the Centre for
  Digital Music; Queen Mary, University of London; and Chris Cannam
  shall not be used in advertising or otherwise to promote the sale,
  use or other dealings in this Software without prior written
  authorization.
*/
 
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
    ProcessQtTransport(std::string processName, std::string formatArg) :
        m_completenessChecker(0) {

        m_process = new QProcess();
        m_process->setReadChannel(QProcess::StandardOutput);
        m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);

        m_process->start(QString::fromStdString(processName),
                         { QString::fromStdString(formatArg) });
        
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
        
        std::cerr << "writing " << size << " bytes to server" << std::endl;
        m_process->write(ptr, size);
        
        std::vector<char> buffer;
        bool complete = false;
        
        while (!complete) {

            qint64 byteCount = m_process->bytesAvailable();

            if (!byteCount) {
                std::cerr << "waiting for data from server..." << std::endl;
                m_process->waitForReadyRead(1000);
                if (m_process->state() == QProcess::NotRunning) {
                    QProcess::ProcessError err = m_process->error();
                    if (err == QProcess::Crashed) {
                        std::cerr << "Server crashed during request" << std::endl;
                    } else {
                        std::cerr << "Server failed during request with error code "
                                  << err << std::endl;
                    }
                    //!!! + catch
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
