#include <string>
#include <vector>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

class File {
    string name;
protected:
    fstream file;
public:
    File(string fileName): name(fileName) {
        file.open(fileName, ios::binary | ios::in | ios::out);

        if (!file.is_open())
            throw runtime_error("Failed to open file: " + fileName);
    }

    const string& filename() { return name; }

    void flush() { file.flush(); }

    // ritorna Record uno dopo l'altro senza un ordine
    virtual istream_iterator<string> getRecords() const = 0;

    // inserisce dati nel database
    virtual void pushData(string_view data) = 0;

    // rimuove un record secondo una chiave
    virtual void deleteData(string_view key) = 0;

    // ritorna un record specifico
    virtual optional<string> getData(string_view key) = 0;

    virtual ~File() = default;
};

using FileRef = reference_wrapper<File>;

class HeapFile: public File {

size_t keySize;
size_t recordSize;
long endFilePosition;

public:
    HeapFile(string fileName, size_t keySize, size_t recordSize)
    : File(fileName), keySize(keySize), recordSize(recordSize) {
        file.seekg(0, ios::end);
        endFilePosition = file.tellg();
    }

    ~HeapFile() override {
        file.seekg(0, ios::beg);
        file.close();
        truncateFile();
    }


    istream_iterator<string> getRecords() const override {
        auto buffer = make_shared<RecordStreamBuffer>(file, recordSize);
        std::istream recordStream(buffer.get());
        return istream_iterator<string>(recordStream);
    }

    void pushData(string_view data) override {

        if(data.length() % recordSize != 0 || data.length() == 0)
            throw runtime_error("Data length is not a multiple of record size");

        file.seekp(endFilePosition, ios::beg);
        file.write(data.data(), data.length());
        endFilePosition += data.length();

    }

    //TODO: permette di eliminare non solo un Record, ma il record che inizia da un certo punto
    void deleteData(string_view key) override {
        if(endFilePosition == 0) return;

        long pos = searchPosition(key);

        if(pos == -1) return;

        file.seekp(pos, ios::beg);
        file.write(getLastRecord().c_str(), recordSize);
        removeLastRecord();
    }

    optional<string> getData(string_view key) {
        string result(recordSize, '\0');

        long pos = searchPosition(key);

        if(pos == -1) return nullopt;

        file.seekg(pos, ios::beg);
        if (file.read((char*)result.data(), recordSize).gcount() == 0)
            return nullopt;

        return result;
    }

private:

    // ritorna -1 se non è stato trovato, un numero positivo altrimenti
    long searchPosition(string_view key) {
        //TODO: gestire il caso in cui la lunghezza della key è sbagliata
        file.seekp(0, ios::beg);
        string data(recordSize, '\0');
        size_t n = 0;

        long result = 0;

        do {
            result += n;
            n = file.read(data.data(), recordSize).gcount();
            if(n < recordSize)
                return -1;
            if(string_view(data.c_str(),keySize) == key)
                return result;
        }while(true);
        
    }

    string getLastRecord() {
        size_t position = endFilePosition - recordSize;

        // Move the file pointer to the position of the last record
        file.seekg(position, ios::beg);

        // Read the last record from the file
        string lastRecord(recordSize, '\0');
        file.read(lastRecord.data(), recordSize);

        return lastRecord;
    }

    void removeLastRecord() {
        endFilePosition -= recordSize;
    }

    void truncateFile() {
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

    class RecordStreamBuffer : public std::streambuf {
        fstream fileStream;
        size_t recordSize;
        string buffer;

    public:
        RecordStreamBuffer(fstream& file, size_t recordSize)
            : fileStream(std::move(file)), recordSize(recordSize) {
            buffer.resize(recordSize);
            this->setg(buffer.data(), buffer.data(), buffer.data());
        }

    protected:
        virtual int underflow() override {
            if (fileStream.eof()) return EOF;

            fileStream.read(buffer.data(), recordSize);
            if (fileStream.gcount() == 0) return EOF;

            this->setg(buffer.data(), buffer.data(), buffer.data() + fileStream.gcount());
            return static_cast<unsigned char>(buffer[0]);
        }
    };

};