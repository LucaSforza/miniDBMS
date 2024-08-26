#include <string>
#include <iterator>
#include <variant>
#include <unordered_map>
#include <stdexcept>
#include <unordered_set>
#include <tuple>
#include <memory>
#include <filesystem>

#include "Domains.cpp"
#include "Files.cpp"

using namespace std;

namespace fs = std::filesystem;


class Field {
    string name;
    SharedDomain domain;
    bool isKeyField;
public:
    Field(const string& name, shared_ptr<Domain> domain,bool isKeyField)
        : name(name), domain(domain), isKeyField(isKeyField) {}

    Field(const string& name, shared_ptr<Domain> domain)
        : name(name), domain(domain), isKeyField(false) {}

    const string& getName() const { return name; }

    SharedDomain getDomain() const { return domain; }

    bool isKey() const { return isKeyField; }

    bool isValid(const string_view value) const {
        return domain->isValid(value);
    }

    size_t size() const {
        return domain.get()->size();
    }

    bool operator==(const Field& other) const {
        return *domain.get() == *other.domain.get() && name == name;
    }
};

using Value = tuple<const Field&,string_view>;

class Relation {
    vector<Field> fields;
    vector<Field> keyFields;
    size_t recordTotalSize;
    size_t keySize;
public:
    Relation(vector<Field> fields) {
        this->fields    = vector<Field>();
        this->keyFields = vector<Field>();

        auto names = unordered_set<string_view>();

        recordTotalSize = 0;
        keySize = 0;
    
        for(Field f: fields) {
            if(f.isKey()) {
                keyFields.push_back(f);
                keySize += f.size();
            } else fields.push_back(f);

            if(names.find(f.getName()) == names.end()) {
                names.insert(f.getName());
            } else {
                throw invalid_argument("Non possono esistere due campi con lo stesso nome");
            }

            recordTotalSize += f.size();

        }

        if(this->keyFields.empty())
            throw invalid_argument("Ci deve essere almeno una chiave");
    }

    /*
        Restituisce il numero di byte laddove inizia in un Record di questa relazione
        un certo Field.
     */
    size_t startPointOf(const Field& field) const {
        size_t result = 0;

        for(auto f : keyFields) {
            if(f.getName() == field.getName()) {
                return result;
            }
            result += f.size();
        }

        for(auto f : fields) {
            if(f.getName() == field.getName()) {
                return result;
            }
            result += f.size();
        }

        return result;
    }

    bool isValid(const string& data) const {

        size_t i = 0;
        bool result = true;

        for(auto f : keyFields) {
            if(!f.isValid(string_view(data.c_str() + i,f.size()))) {
                return false;
            }
            i += f.size();
        }

        for(auto f : fields) {
            if(!f.isValid(string_view(data.c_str() + i,f.size()))) {
                return false;
            }
            i += f.size();
        }

        return true;
    }

    const vector<Field>& getKey() const {
        return keyFields;
    }

    bool operator==(const Relation& other) const {
        return fields == other.fields && keyFields == other.keyFields;
    }

    size_t getRecordSize() { return recordTotalSize; }

    size_t getKeySize() { return keySize; }

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

    Record(shared_ptr<Relation> rel, string data): rel(rel), data(data) {

        if(!rel.get()->isValid(data)) {
            throw invalid_argument("I dati non sono validi");
        }
    }

    const string& getData() { return data; }

    // Ritorna una vista sulla parte di record di cui fa parte il campo
    const string_view valueAt(const Field& field) const {
        return string_view(data.c_str() + rel.get()->startPointOf(field), field.size());
    }

    bool valuesInside(const vector<Value>& values) const {
        for(auto [field,data] : values) {
            if(valueAt(field) != data)
                return false;
        }

        return true;
    }

    bool isValid() const {
        return rel.get()->isValid(data);
    }

    vector<Value> getKey() const {

        auto result = vector<Value>();

        for(auto key : rel.get()->getKey()) {
            result.push_back(tuple(key,valueAt(key))); //TODO: fare in modo che l'interno del for sia O(1)
        }

        return result;
    }

    string_view getKeyData() const {
        return string_view(data.c_str(), rel.get()->getKeySize());
    }
};

using ConstRecordRef = reference_wrapper<const Record>;

class Table {
    string name;
    shared_ptr<Relation> rel;
    // records salvati nella RAM e non sul disco rigito
    vector<Record> volatileRecords;
    SharedFile file;
public:
    Table(shared_ptr<Relation> rel, string name, SharedFile file): rel(rel), name(name),file(file) {}

    void addRecord(Record record) {
        if(!search(record.getKey()).empty()) {
            throw invalid_argument("Vincolo di chiave infranto");
        }
        auto f = file.get();
        if(f->getData(record.getKeyData())) //TODO: inserire questo nella search
            throw runtime_error("Record già esistente"); //TODO: dopo aver eseguito getData poi il file è rotto
        f->pushData(record.getData());
        f->flush();
    }

    vector<ConstRecordRef> search(const vector<Value>& values) const {
        auto result = vector<ConstRecordRef>();

        for(const Record& r : volatileRecords) {
            if(r.valuesInside(values))
                result.push_back(r);
        }

        return result;
    }

    const string& getName() const {
        return name;
    }

    shared_ptr<Relation> getRelation() { return rel; }

    void flush() { volatileRecords.clear(); }
};

using TableRef = reference_wrapper<Table>;

class Database {
    string name;
    string dirPath;
    vector<SharedDomain> domains;
    vector<Table> tables;
public:
    Database(string name,string dirPath): name(name), dirPath(dirPath) {
        domains.push_back(make_shared<IntegerDomain>());
        domains.push_back(make_shared<StringDomain>(25));

        if (!fs::exists(dirPath)) {
            fs::create_directory(dirPath);
        }
    }

    void addDomain(SharedDomain domain) {
        //TODO: controllare che il dominio sia unico all'interno del database
        domains.push_back(domain);
    }

    void addTable(string name, shared_ptr<Relation> relation) {
        // TODO: Check if all domains in the table's relation exist in the database

        // Check if the table name is unique in the database
        for (Table& existingTable : tables) {
            if (existingTable.getName() == name) {
                throw invalid_argument("Table name already exists in the database");
            }
        }

        //TODO: permettere di far scegliere il tipo di file da utilizzare, per ora tutte le tabelle saranno HeapFile
        // in futuro l'utente potrà scegliere tra: HeapFile, HashFile, BTreeFile, IndexFile
        auto file = make_shared<HeapFile>(
            dirPath + name,relation.get()->getKeySize(),relation.get()->getRecordSize()
        );

        tables.push_back(Table(relation,name,file));
    }

    optional<TableRef> getTable(string_view name) {
        for (Table& table : tables) {
            if (table.getName() == name) {
                return table;
            }
        }
        return nullopt;
    }

    // ritorna True se esisteva una tabella con quel nome, False se la tabella non esisteva
    bool deleteTable(string_view name) {
        for (auto it = tables.begin(); it != tables.end(); ++it) {
            if (it->getName() == name) {
                tables.erase(it);
                return true;
            }
        }
        return false;
    }
};