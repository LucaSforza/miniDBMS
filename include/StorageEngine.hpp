#ifndef STORAGEENGINE_HPP
#define STORAGEENGINE_HPP

#include <string>
#include <iterator>
#include <variant>
#include <unordered_map>
#include <stdexcept>
#include <unordered_set>
#include <tuple>
#include <memory>
#include <filesystem>

#include "Domains.hpp"
#include "File.hpp"
#include "HeapFile.hpp"
#include "Tables.hpp"

using namespace std;

namespace fs = std::filesystem;


class Field {
    string name;
    SharedDomain domain;
    bool isKeyField;
public:
    Field(const string& name, shared_ptr<Domain> domain,bool isKeyField);

    Field(const string& name, shared_ptr<Domain> domain);

    const string& getName() const;

    SharedDomain getDomain() const;

    bool isKey() const;

    bool isValid(const string_view value) const;

    size_t size() const;

    bool operator==(const Field& other) const;
};

using Value = tuple<const Field&,string_view>;

class Relation {
    vector<Field> fields;
    vector<Field> keyFields;
    size_t recordTotalSize;
    size_t keySize;
public:
    Relation(vector<Field> fields);

    /*
        Restituisce il numero di byte laddove inizia in un Record di questa relazione
        un certo Field.
     */
    size_t startPointOf(const Field& field) const;

    bool isValid(const string& data) const;

    const vector<Field>& getKey() const;

    bool operator==(const Relation& other) const;

    size_t getRecordSize() const;

    size_t getKeySize() const;

    /* vector<Field>::iterator getFields() const {
        //TODO: implementare
    } */

};

/*
    I Record sono fatti nel seguente modo:

    Prima i Field della chiave nell'ordine definito
    dalla Relazione.
    Poi tutti i field nell'ordine definito dalla relazione.
    Tutti i campi sono attaccati.
*/
class Record {
    shared_ptr<Relation> rel;
    string data;
public:

    Record(shared_ptr<Relation> rel, string data);

    const string& getData();

    // Ritorna una vista sulla parte di record di cui fa parte il campo
    const string_view valueAt(const Field& field) const;

    bool valuesInside(const vector<Value>& values) const;

    bool isValid() const;

    vector<Value> getKey() const;

    string_view getKeyData() const;

    void setValue(const Value& val);

};

using ConstRecordRef = reference_wrapper<const Record>;

class Database {
    string name;
    string dirPath;
    vector<SharedDomain> domains;
    vector<PhysicalTable> tables;
public:
    Database(string name,string dirPath);

    void addDomain(SharedDomain domain);

    template<typename F,typename = enable_if_t<is_base_of<File, F>::value>>
    void addTable(string name, shared_ptr<Relation> relation);

    optional<PhysicalTableRef> getTable(string_view name);

    // ritorna True se esisteva una tabella con quel nome, False se la tabella non esisteva
    bool deleteTable(string_view name);
};

#endif // STORAGEENGINE_HPP