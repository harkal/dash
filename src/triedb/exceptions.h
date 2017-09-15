#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <boost/exception/exception.hpp>

struct Exception: virtual std::exception, virtual boost::exception
{
    Exception(const std::string& message = std::string()): mMessage(std::move(message)) {}
    const char* what() const noexcept override { return mMessage.empty() ? std::exception::what() : mMessage.c_str(); }

private:
    std::string mMessage;
};

#define DEV_SIMPLE_EXCEPTION(X) struct X: virtual Exception { const char* what() const noexcept override { return #X; } }

DEV_SIMPLE_EXCEPTION(RootNotFound);
struct BadRoot: virtual Exception { public: BadRoot(const H256& root): Exception("BadRoot " + root.GetHex()), mRoot(root) {} H256 mRoot; };

#endif // EXCEPTIONS_H
