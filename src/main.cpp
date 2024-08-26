#include <iostream>

#include "StorageEngine.cpp"

int main(void) {

    auto db = Database("prova","../databases/prova/");

    auto fields = vector<Field>();

    fields.push_back(Field("Chiave",make_shared<IntegerDomain>(),true));
    fields.push_back(Field("Nome",make_shared<StringDomain>(25)));

    auto rel = make_shared<Relation>(Relation(fields));

    db.addTable("Principale",rel);

    auto table = db.getTable("Principale");

    Table& t = table.value().get();

    auto data = string("\2\1\1\1Ciao\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1");


    cout << data.length() << endl;

    t.addRecord(Record(rel,data));

    return 0;
}