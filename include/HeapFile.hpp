#ifndef HEAPFILE_HPP
#define HEAPFILE_HPP

#include "File.hpp"

class HeapFile: public File {

size_t keySize;
size_t recordSize;
long endFilePosition;

public:
    HeapFile(string fileName, size_t keySize, size_t recordSize);

    ~HeapFile() override;

    iterator<input_iterator_tag,string> begin() override;
    iterator<input_iterator_tag,string> end() override;
    void pushData(string_view data) override;
    void deleteData(string_view key) override;
    optional<string> getData(string_view key) override;

private:

    // ritorna -1 se non è stato trovato, un numero positivo altrimenti
    long searchPosition(string_view key);
    string getLastRecord();
    void removeLastRecord();
    void truncateFile();

};

#endif // HEAPFILE_HPP