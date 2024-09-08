#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>

#include "HeapFile.hpp"
#include "File.hpp"


using namespace std;

File::File(string fileName): name(fileName) {
    file.open(fileName, ios::binary | ios::in | ios::out);

    if (!file.is_open()) {
        file.open(fileName, ios::binary | ios::in | ios::out | ios::trunc);
        if(!file.is_open())
            throw runtime_error("Failed to open file: " + fileName);
    }
}

const string& File::filename() const { return name; }

void File::flush() { file.flush(); }

void File::sync() { file.sync(); }


HeapFile::HeapFile(string fileName, size_t keySize, size_t recordSize)
: File(fileName), keySize(keySize), recordSize(recordSize) {
    file.seekg(0, ios::end);
    endFilePosition = file.tellg();
}

HeapFile::~HeapFile() {
    file.close();
    truncateFile();
}

class RecordIterator : public iterator<input_iterator_tag, string> {
    fstream& fileStream;
    size_t recordSize;
    size_t pos;
public:
    RecordIterator(fstream& file, size_t recordSize, size_t pos = 0)
        : fileStream(file), recordSize(recordSize), pos(pos) {}

    iterator& operator++() {
        pos += recordSize;
        return *this;
    }

    string operator*() const {
        string record(recordSize, '\0');
        fileStream.seekg(pos, ios::beg);
        fileStream.read(record.data(), recordSize);
        return record;
    }

    bool operator==(const RecordIterator& other) const {
        return pos == other.pos;
    }

    bool operator!=(const RecordIterator& other) const {
        return pos != other.pos;
    }
};

iterator<input_iterator_tag,string> HeapFile::begin() {
    return RecordIterator(file, recordSize);
}

iterator<input_iterator_tag,string> HeapFile::end() {
    return RecordIterator(file, recordSize, endFilePosition);
}

void HeapFile::pushData(string_view data) {
    if(data.length() % recordSize != 0 || data.length() == 0)
        throw runtime_error("Data length is not a multiple of record size");

    file.seekg(endFilePosition, ios::beg);
    if(!file.write(data.data(), data.length()))
        throw runtime_error("Failed to write data");
    endFilePosition += data.length();
}

optional<string> HeapFile::deleteData(string_view key) {
    
    auto last_record = getLastRecord();
    
    if(last_record.has_value()) {

        long pos = searchPosition(key);
        if(pos == -1) return nullptr;

        file.seekp(pos, ios::beg);
        file.write(last_record.value().c_str(), recordSize);
        removeLastRecord();
        return last_record.value();
    }

    return nullptr;
}

optional<string> HeapFile::getData(string_view key) {
    string result(recordSize, '\0');

    file.seekg(0, ios::beg);
    size_t n = 0;

    do {
        n = file.read(result.data(), recordSize).gcount();
        if(n < recordSize) {
            file.clear();
            return nullopt;
        }
        if(string_view(result.c_str(),keySize) == key)
            return result;
    }while(true);

    throw std::logic_error("FATAL ERROR: Unreachable code");
}

long HeapFile::searchPosition(string_view key) {
    //TODO: gestire il caso in cui la lunghezza della key è sbagliata

    if(!file)
        throw runtime_error("Not working");

    file.seekg(0, ios::beg);
    string data(recordSize, '\0');
    size_t n = 0;

    long result = 0;

    do {
        result += n;
        n = file.read(data.data(), recordSize).gcount();
        if(n < recordSize) {
            file.clear();
            return -1;
        }
        if(string_view(data.c_str(),keySize) == key)
            return result;
    }while(true);

        
}

optional<string> HeapFile::getLastRecord() {
    // TODO: gestire il caso in cui non ci sia un record alla fine poiché il file è vuoto
    size_t position = endFilePosition - recordSize;

        // Move the file pointer to the position of the last record
    file.seekg(position, ios::beg);

        // Read the last record from the file
    string lastRecord(recordSize, '\0');
    file.read(lastRecord.data(), recordSize);


    return lastRecord;
}

void HeapFile::removeLastRecord() {
    endFilePosition -= recordSize;
}

void HeapFile::truncateFile() {
    //TODO: modificare questo metodo in modo che sia supportato anche su Windows

    int fd = open(filename().c_str(), O_RDWR); // Ottieni il file descriptor
    if (fd == -1) {
        throw runtime_error("Failed to open file descriptor: " + filename());
    }

    if (ftruncate(fd, endFilePosition) != 0) {
        close(fd);
        throw runtime_error("Failed to truncate file: " + filename());
    }

    close(fd);
}