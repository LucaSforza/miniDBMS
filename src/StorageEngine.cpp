#include <string>
#include <iterator>
#include <variant>
#include <unordered_map>
#include <stdexcept>
#include <unordered_set>
#include <tuple>

#include "Domains.cpp"

using namespace std;

class Field {
public:
    Field(const string& name, shared_ptr<Domain> domain,bool isKeyField)
        : name(name), domain(domain), isKeyField(isKeyField) {}

    Field(const string& name, shared_ptr<Domain> domain)
        : name(name), domain(domain), isKeyField(false) {}

    const string& getName() const { return name; }

    SharedDomain getDomain() { return domain; }

    bool isKey() { return isKeyField; }

    bool isValid(const string_view value) const {
        return domain->isValid(value);
    }

    size_t size() const {
        return domain.get()->size();
    }

private:
    string name;
    SharedDomain domain;
    bool isKeyField;
};

//TODO: Field deve essere un puntatore
using Value = tuple<const Field&,string_view>;

class Relation {
public:
    Relation(vector<Field> fields) {
        this->fields    = vector<Field>();
        this->keyFields = vector<Field>();

        auto names = unordered_set<string_view>();
    
        for(Field f: fields) {
            if(f.isKey())
                keyFields.push_back(f);
            else fields.push_back(f);

            if(names.find(f.getName()) == names.end()) {
                names.insert(f.getName());
            } else {
                throw invalid_argument("Non possono esistere due campi con lo stesso nome");
            }
        }

        if(this->keyFields.empty())
            throw invalid_argument("Ci deve essere almeno una chiave");
    }

    /*
        Restituisce il numero di byte laddove inizia in un Record di questa relazione
        un certo Field.
     */
    size_t startPointOf(const Field& field) {
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

    bool isValid(const string& data) {

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

    const vector<Field>& getKey() {
        return keyFields;
    }

    bool operator==(const Relation& other) const {
        return fields == other.fields && keyFields == other.keyFields;
    }

    vector<Field>::iterator getFields() {
        //TODO: implementare
    }

private:
    vector<Field> fields;
    vector<Field> keyFields;
};

/*
    I Record sono fatti nel seguente modo:

    Prima i Field della chiave nell'ordine definito
    dalla Relazione.
    Poi tutti i field nell'ordine definito dalla relazione.
    Tutti i campi sono attaccati.
*/
class Record {
public:

    Record(shared_ptr<Relation> rel, string data): rel(rel), data(data) {
        if(!rel.get()->isValid(data)) {
            throw invalid_argument("I dati non sono validi");
        }
    }

    // Ritorna una vista sulla parte di record di cui fa parte il campo
    const string_view valueAt(const Field& field) {
        return string_view(data.c_str() + rel.get()->startPointOf(field), field.size());
    }

    bool valuesInside(const vector<Value>& values) {
        for(auto [field,data] : values) {
            if(valueAt(field) != data)
                return false;
        }

        return true;
    }

    bool isValid() {
        return rel.get()->isValid(data);
    }

    vector<Value> getKey() {

        auto result = vector<Value>();

        for(const Field& key : rel.get()->getKey()) {
            result.push_back(tuple(key,valueAt(key))); //TODO: fare in modo che l'interno del for sia O(1)
        }

        return result;
    }

private:
    shared_ptr<Relation> rel;
    string data;
};


class Table {
public:
    Table(shared_ptr<Relation> rel, string name): rel(rel), name(name) {}

    void addRecord(Record record) {
        if(!search(record.getKey()).empty()) {
            throw invalid_argument("Vincolo di chiave infranto");
        }
        volatileRecords.push_back(record);
    }

    vector<Record&> search(const vector<Value>& values) {
        auto result = vector<Record&>();

        for(Record& r : volatileRecords) {
            if(r.valuesInside(values))
                result.push_back(r);
        }

        return result;
    }

    const string& getName() {
        return name;
    }

    shared_ptr<Relation> getRelation() { return rel; }

private:
    string name;
    shared_ptr<Relation> rel;
    // records salvati nella RAM e non sul disco rigito
    vector<Record> volatileRecords;
};

class Database {
public:
    Database() {}

    Database(vector<SharedDomain> domains): domains(domains) {}

    void addDomain(SharedDomain domain) {
        //TODO: controllare che il dominio sia unico all'interno del database
        domains.push_back(domain);
    }

    void addTable(Table table) {
        // TODO: Check if all domains in the table's relation exist in the database

        // Check if the table name is unique in the database
        for (Table& existingTable : tables) {
            if (existingTable.getName() == table.getName()) {
                throw invalid_argument("Table name already exists in the database");
            }
        }

        tables.push_back(table);
    }

    optional<Table&> getTable(string_view name) {
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

private:
    vector<SharedDomain> domains;
    vector<Table> tables;
};