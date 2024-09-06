#ifndef DOMAINS_HPP
#define DOMAINS_HPP

#include <string>
#include <vector>

/**
 * @brief The base class for all domains in the miniDBMS.
 * 
 * This class defines the common interface for all domains in the miniDBMS.
 * It provides pure virtual functions for validating a value, getting the size of the domain,
 * and comparing two domains for equality.
 * 
 * @note This class is meant to be inherited from and should not be instantiated directly.
 */
class Domain {
public:
    virtual ~Domain() = default;
    /**
     * @brief check if a piece of data validate the domain costraints
     */
    virtual bool isValid(const std::string_view value) const = 0;
    /**
     * @return Size required in bytes to save data in memory
     */
    virtual size_t size() const = 0;
    virtual bool operator==(const Domain& other) const = 0;

};

using SharedDomain = std::shared_ptr<Domain>;

/**
 * @class EnumDomain
 * @brief Represents a domain with a fixed set of valid values.
 * 
 * This class inherits from the Domain class and provides functionality for working with domains that have a fixed set of valid values.
 * 
 */
class EnumDomain : public Domain {
    std::vector<std::string> validValues;
    size_t max_len;
public:
    EnumDomain(const std::vector<std::string>& validValues);
    bool isValid(const std::string_view value) const override;
    size_t size() const override;
    bool operator==(const Domain& other) const override;
};

/**
 * @class IntegerDomain
 * @brief Represents a domain for integer values.
 * 
 * This class inherits from the Domain base class and provides functionality for validating integer values
 */
class IntegerDomain : public Domain {
public:
    bool isValid(const std::string_view value) const override;
    size_t size() const override;
    bool operator==(const Domain& other) const override;
};

/**
 * @class StringDomain
 * @brief Represents a domain for string values.
 * 
 * The StringDomain class is a subclass of the Domain class and is used to define a domain for string values.
 * It allows specifying a maximum length for the strings in the domain.
 */
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