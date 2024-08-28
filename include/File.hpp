#ifndef FILE_HPP
#define FILE_HPP

#include <string>
#include <fstream>
#include <optional>

using namespace std;

class File {
    string name;
protected:
    fstream file;
public:
    File(string fileName);
    virtual ~File() = default;

    const string& filename() const;
    void flush();
    void sync();

    // ritorna Record uno dopo l'altro senza un ordine
    virtual iterator<input_iterator_tag,string> begin() = 0;
    // ritorna ultimo iteratore possibile
    virtual iterator<input_iterator_tag,string> end() = 0;
    // inserisce dati nel database
    virtual void pushData(string_view data) = 0;
    // rimuove un record secondo una chiave
    virtual void deleteData(string_view key) = 0;
    // ritorna un record specifico
    virtual optional<string> getData(string_view key) = 0;

};

using SharedFile = shared_ptr<File>;

#endif // FILE_HPP