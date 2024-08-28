#ifndef DOMAINS_HPP
#define DOMAINS_HPP

#include <string>
#include <vector>

// Classe base per un dominio
class Domain {
public:
    virtual ~Domain() = default;
    virtual bool isValid(const std::string_view value) const = 0;
    virtual size_t size() const = 0;
    virtual bool operator==(const Domain& other) const = 0;

};

using SharedDomain = std::shared_ptr<Domain>;

class EnumDomain : public Domain {
    std::vector<std::string> validValues;
    size_t max_len;
public:
    EnumDomain(const std::vector<std::string>& validValues);
    bool isValid(const std::string_view value) const override;
    size_t size() const override;
    bool operator==(const Domain& other) const override;
};

class IntegerDomain : public Domain {
public:
    bool isValid(const std::string_view value) const override;
    size_t size() const override;
    bool operator==(const Domain& other) const override;
};

class StringDomain : public Domain {
    size_t max_len;
public:
    StringDomain(size_t max_len);
    bool isValid(const std::string_view value) const override;
    size_t size() const override;
    bool operator==(const Domain& other) const override;
};

//TODO: aggiungere domini

#endif // DOMAINS_HPP