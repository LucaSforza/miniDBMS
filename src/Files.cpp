#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>

#include "HeapFile.hpp"
#include "File.hpp"


using namespace std;

// -----------------------------------------------------------------------
// File (base)
// -----------------------------------------------------------------------

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

// -----------------------------------------------------------------------
// HeapFile
// -----------------------------------------------------------------------

HeapFile::HeapFile(string fileName, size_t keySize, size_t recordSize)
: File(fileName), keySize(keySize), recordSize(recordSize) {
    // Validate basic size contracts
    if (recordSize == 0)
        throw runtime_error("HeapFile: recordSize must be non-zero");
    if (keySize == 0)
        throw runtime_error("HeapFile: keySize must be non-zero");
    if (keySize > recordSize)
        throw runtime_error("HeapFile: keySize must not exceed recordSize");

    // Validate that an existing file is well-formed (multiple of recordSize)
    file.clear();
    file.seekg(0, ios::end);
    streampos length = file.tellg();
    if (length > 0 && (length % recordSize) != 0)
        throw runtime_error("HeapFile: existing file size is not a multiple of recordSize");

    endFilePosition = static_cast<long>(length);
}

HeapFile::~HeapFile() {
    // Destructor performs no throwing persistence work.
    // Physical truncation happens eagerly in deleteData.
    if (file.is_open())
        file.close();
}

// -----------------------------------------------------------------------
// scan — return all records as a vector
// -----------------------------------------------------------------------

vector<string> HeapFile::scan() {
    vector<string> records;
    file.clear();
    file.seekg(0, ios::beg);

    string record(recordSize, '\0');
    while (file.read(record.data(), recordSize))
        records.push_back(record);

    file.clear();               // clear EOF/fail bits for subsequent operations
    return records;
}

// -----------------------------------------------------------------------
// insert — append a single fixed-width record
// -----------------------------------------------------------------------

void HeapFile::insert(string_view record) {
    if (record.size() != recordSize)
        throw runtime_error("HeapFile::insert: record size does not match expected recordSize");

    file.clear();
    file.seekp(0, ios::end);
    if (!file.write(record.data(), recordSize))
        throw runtime_error("HeapFile::insert: failed to write record");

    endFilePosition += recordSize;
}

// -----------------------------------------------------------------------
// getData — search for a record by key prefix; return nullopt if missing
// -----------------------------------------------------------------------

optional<string> HeapFile::getData(string_view key) {
    if (key.size() != keySize)
        throw runtime_error("HeapFile::getData: key size mismatch");

    file.clear();
    file.seekg(0, ios::beg);

    string record(recordSize, '\0');
    while (file.read(record.data(), recordSize)) {
        if (string_view(record.data(), keySize) == key) {
            file.clear();
            return record;
        }
    }

    file.clear();
    return nullopt;
}

// -----------------------------------------------------------------------
// updateData — overwrite the full record identified by key
// -----------------------------------------------------------------------

void HeapFile::updateData(string_view key, string_view newRecord) {
    if (key.size() != keySize)
        throw runtime_error("HeapFile::updateData: key size mismatch");
    if (newRecord.size() != recordSize)
        throw runtime_error("HeapFile::updateData: new record size mismatch");

    long pos = searchPosition(key);
    if (pos == -1)
        throw runtime_error("HeapFile::updateData: key not found");

    file.clear();
    file.seekp(pos, ios::beg);
    if (!file.write(newRecord.data(), recordSize))
        throw runtime_error("HeapFile::updateData: failed to write record");
}

// -----------------------------------------------------------------------
// deleteData — remove the record identified by key, return the deleted record
//
// The implementation uses swap-with-last: the last record fills the vacated
// slot, then the file is truncated immediately.
// -----------------------------------------------------------------------

optional<string> HeapFile::deleteData(string_view key) {
    if (key.size() != keySize)
        throw runtime_error("HeapFile::deleteData: key size mismatch");

    long pos = searchPosition(key);
    if (pos == -1)
        return nullopt;

    // Read the record at the key position (the one to delete)
    file.clear();
    file.seekg(pos, ios::beg);
    string deletedRecord(recordSize, '\0');
    if (!file.read(deletedRecord.data(), recordSize))
        throw runtime_error("HeapFile::deleteData: failed to read record for deletion");

    // If the deleted record is not the last one, swap the last record into its place
    bool isLast = (pos == static_cast<long>(endFilePosition - recordSize));
    if (!isLast) {
        auto last = getLastRecord();
        if (!last)
            throw runtime_error("HeapFile::deleteData: inconsistent state (no last record)");

        file.clear();
        file.seekp(pos, ios::beg);
        if (!file.write(last->data(), recordSize))
            throw runtime_error("HeapFile::deleteData: failed to write swap record");
    }

    // Remove the last slot and truncate the physical file immediately
    removeLastRecord();
    return deletedRecord;
}

// -----------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------

long HeapFile::searchPosition(string_view key) {
    if (key.size() != keySize)
        throw runtime_error("HeapFile::searchPosition: key size mismatch");

    file.clear();
    file.seekg(0, ios::beg);

    string data(recordSize, '\0');
    long pos = 0;

    while (file.read(data.data(), recordSize)) {
        if (string_view(data.data(), keySize) == key) {
            file.clear();
            return pos;
        }
        pos += recordSize;
    }

    file.clear();
    return -1;
}

optional<string> HeapFile::getLastRecord() {
    if (endFilePosition == 0)
        return nullopt;

    file.clear();
    file.seekg(endFilePosition - recordSize, ios::beg);

    string record(recordSize, '\0');
    if (!file.read(record.data(), recordSize))
        return nullopt;

    return record;
}

void HeapFile::removeLastRecord() {
    if (endFilePosition == 0)
        throw runtime_error("HeapFile::removeLastRecord: heap is empty");

    endFilePosition -= recordSize;
    truncateFile();
}

void HeapFile::truncateFile() {
    // Flush the fstream buffer so the OS-level truncation sees the latest data.
    // ftruncate is POSIX; this DBMS targets Linux (and other POSIX systems).
    file.flush();

    int fd = open(filename().c_str(), O_RDWR);
    if (fd == -1)
        throw runtime_error("Failed to open file descriptor for truncation: " + filename());

    if (ftruncate(fd, endFilePosition) != 0) {
        close(fd);
        throw runtime_error("Failed to truncate file: " + filename());
    }

    close(fd);
}