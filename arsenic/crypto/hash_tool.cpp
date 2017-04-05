/*
 * This file is part of Arsenic.
 *
 * Copyright (C) 2017 Corraire Fabrice <antidote1911@gmail.com>
 *
 * Arsenic is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * Arsenic is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Arsenic. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hash_tool.h"
#include <fstream>
#include <QtCore>

#if defined(Q_OS_LINUX)
 #if defined(__clang__)
   #include "botan/clang/botan_all.h"
 #elif defined(__GNUC__) || defined(__GNUG__)
   #include "botan/gcc/botan_all.h"
#endif
#endif

#if defined(Q_OS_WIN64)
  #include "botan/msvc_x64/botan_all.h"
#endif



Hash_Tool::Hash_Tool(QObject *parent) :
    QThread(parent)
{
}

void Hash_Tool::run()
{
    calculateHash(m_filenames, m_algo);

    Hash_Tool::exit();

}

void Hash_Tool::setParam(QStringList filenames, QString algo)
{
    m_filenames    = filenames;
    m_algo        = algo;
}

void Hash_Tool::calculateHash(QStringList& filenames, QString algo)
{
    for (auto& filename : filenames)
    {
        std::ifstream stream{filename.toStdString(), std::ios::binary};

        if(stream.bad() || (stream.fail() && !stream.eof()))
        {
            emit statusResult(algo,"Stream_IO_Error: Can't read file.", filename);
            return;
        }

        emit PercentProgressHashChanged(filename,0,"hash_object");


        std::unique_ptr<Botan::HashFunction> hash(Botan::HashFunction::create(algo.toStdString()));

        // Define a size for the buffer vector
        const auto bufferSize = static_cast<std::size_t>(4096);
        Botan::secure_vector<Botan::byte> buffer(bufferSize);

        // Get file size for percent progress calculation
        QFileInfo file{filename};
        const qint64 size = file.size();
        std::size_t fileIndex = 0;
        qint64 percent = -1;

        while(stream.good() && aborted==false)
        {
            stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            const auto remainingSize = stream.gcount();
            hash->update(buffer.data(),remainingSize);

            // Calculate progress in percent
            fileIndex += remainingSize;
            const auto nextFraction = static_cast<double>(fileIndex) / static_cast<double>(size);
            const qint64 nextPercent = static_cast<qint64>(nextFraction * 100);

            if (nextPercent > percent && nextPercent < 100)
            {
                percent = nextPercent;
                emit PercentProgressHashChanged(filename, percent,"hash_object");
            }
        }

        stream.close();

        QString result = QString::fromStdString(Botan::hex_encode(hash->final()));

        emit PercentProgressHashChanged(filename,100,"hash_object");
        emit statusResult(algo,result,filename);
    }

}
