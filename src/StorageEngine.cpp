#include <string>
#include <iterator>
#include <variant>
#include <unordered_map>
#include <stdexcept>
#include <unordered_set>

#include "Domains.cpp"

using namespace std;

class Field {
public:
    Field(const string& name, shared_ptr<Domain> domain,bool isKeyField)
        : name(name), domain(domain), isKeyField(isKeyField) {}

    Field(const string& name, shared_ptr<Domain> domain)
        : name(name), domain(domain), isKeyField(false) {}

    const string& getName() const { return name; }

    bool isValid(const string_view value) const {
        return domain->isValid(value);
    }

    bool isKey() { return isKeyField; }

    uint size() {
        return domain.get()->size();
    }

private:
    string name;
    shared_ptr<Domain> domain;
    bool isKeyField;
};

class Relation {
public:
    Relation(vector<Field> fields) {
        this->fields    = vector<Field>();
        this->keyFields = vector<Field>();

        auto names = unordered_set<string>();
    
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
    size_t startPointOf(Field& field) {
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

    bool operator==(const Relation& other) const {
        return fields == other.fields && keyFields == other.keyFields;
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
    //TODO:
    Record(shared_ptr<Relation> rel, string data): rel(rel), data(data) {
        if(!rel.get()->isValid(data)) {
            throw invalid_argument("I dati non sono validi");
        }
    }

    // Ritorna una vista sulla parte di record di cui fa parte il campo
    const string_view valueAt(Field& field) {
        return string_view(data.c_str() + rel.get()->startPointOf(field), field.size());
    }

    bool isValid() {
        return rel.get()->isValid(data);
    }

private:
    shared_ptr<Relation> rel;
    string data;
};

class Table {
//TODO: aggiungere concetto di chiave
public:
    Table(shared_ptr<Relation> rel): rel(rel) {}

    void addRecord(Record record) {
        if(!record.isValid())
            throw invalid_argument("Record non valido");
        volatileRecords.push_back(record);
    }

private:
    shared_ptr<Relation> rel;
    // records salvati nella RAM e non sul disco rigito
    vector<Record> volatileRecords;
};