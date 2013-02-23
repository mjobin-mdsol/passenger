/*
 *  Phusion Passenger - https://www.phusionpassenger.com/
 *  Copyright (c) 2010-2013 Phusion
 *
 *  "Phusion Passenger" is a trademark of Hongli Lai & Ninh Bui.
 *
 *  See LICENSE file for license information.
 */
#ifndef _PASSENGER_EXCEPTIONS_H_
#define _PASSENGER_EXCEPTIONS_H_

#include <oxt/tracable_exception.hpp>
#include <string>
#include <map>
#include <sstream>
#include <cstring>
#include <cassert>

/**
 * @defgroup Exceptions Exceptions
 */

namespace Passenger {

using namespace std;

/**
 * Represents an error returned by a system call or a standard library call.
 *
 * Use the code() method to find out the value of <tt>errno</tt> at the time
 * the error occured.
 *
 * @ingroup Exceptions
 */
class SystemException: public oxt::tracable_exception {
private:
	string briefMessage;
	string systemMessage;
	string fullMessage;
	int m_code;
public:
	/**
	 * Create a new SystemException.
	 *
	 * @param briefMessage A brief message describing the error.
	 * @param errorCode The error code, i.e. the value of errno right after the error occured.
	 * @note A system description of the error will be appended to the given message.
	 *    For example, if <tt>errorCode</tt> is <tt>EBADF</tt>, and <tt>briefMessage</tt>
	 *    is <em>"Something happened"</em>, then what() will return <em>"Something happened: Bad
	 *    file descriptor (10)"</em> (if 10 is the number for EBADF).
	 * @post code() == errorCode
	 * @post brief() == briefMessage
	 */
	SystemException(const string &briefMessage, int errorCode) {
		stringstream str;
		
		str << strerror(errorCode) << " (errno=" << errorCode << ")";
		systemMessage = str.str();
		
		setBriefMessage(briefMessage);
		m_code = errorCode;
	}
	
	virtual ~SystemException() throw() {}
	
	virtual const char *what() const throw() {
		return fullMessage.c_str();
	}
	
	void setBriefMessage(const string &message) {
		briefMessage = message;
		fullMessage = briefMessage + ": " + systemMessage;
	}
	
	/**
	 * The value of <tt>errno</tt> at the time the error occured.
	 */
	int code() const throw() {
		return m_code;
	}
	
	/**
	 * Returns a brief version of the exception message. This message does
	 * not include the system error description, and is equivalent to the
	 * value of the <tt>message</tt> parameter as passed to the constructor.
	 */
	string brief() const throw() {
		return briefMessage;
	}
	
	/**
	 * Returns the system's error message. This message contains both the
	 * content of <tt>strerror(errno)</tt> and the errno number itself.
	 */
	string sys() const throw() {
		return systemMessage;
	}
};

/**
 * A filesystem error, as returned by the operating system. This may include,
 * for example, permission errors.
 *
 * @ingroup Exceptions
 */
class FileSystemException: public SystemException {
private:
	string m_filename;
public:
	FileSystemException(const string &message, int errorCode,
		const string &filename)
		: SystemException(message, errorCode),
		  m_filename(filename) {}
	
	virtual ~FileSystemException() throw() {}
	
	/**
	 * The filename that's associated to the error.
	 */
	string filename() const throw() {
		return m_filename;
	}
};

/**
 * Unable to retrieve the system time using <tt>time()</tt>.
 *
 * @ingroup Exceptions
 */
class TimeRetrievalException: public SystemException {
public:
	TimeRetrievalException(const string &message, int errorCode)
		: SystemException(message, errorCode)
		{}
	virtual ~TimeRetrievalException() throw() {}
};

/**
 * Represents an error that occured during an I/O operation.
 *
 * @ingroup Exceptions
 */
class IOException: public oxt::tracable_exception {
private:
	string msg;
public:
	IOException(const string &message): msg(message) {}
	virtual ~IOException() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

/**
 * Thrown when a certain file cannot be found.
 */
class FileNotFoundException: public IOException {
public:
	FileNotFoundException(const string &message): IOException(message) {}
	virtual ~FileNotFoundException() throw() {}
};

/**
 * An unexpected end-of-file I/O error.
 *
 * @ingroup Exceptions
 */
class EOFException: public IOException {
public:
	EOFException(const string &message): IOException(message) {}
	virtual ~EOFException() throw() {}
};

/**
 * Thrown when an invalid configuration is given.
 */
class ConfigurationException: public oxt::tracable_exception {
private:
	string msg;
public:
	ConfigurationException(const string &message): msg(message) {}
	virtual ~ConfigurationException() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

/**
 * Thrown when SpawnManager or ApplicationPool fails to spawn an application
 * instance. The exception may contain an error page, which is a user-friendly
 * HTML page with details about the error.
 */
class SpawnException: public oxt::tracable_exception {
public:
	enum ErrorKind {
		UNDEFINED_ERROR,
		PRELOADER_STARTUP_PROTOCOL_ERROR,
		PRELOADER_STARTUP_TIMEOUT,
		PRELOADER_STARTUP_EXPLAINABLE_ERROR,
		APP_STARTUP_PROTOCOL_ERROR,
		APP_STARTUP_TIMEOUT,
		APP_STARTUP_EXPLAINABLE_ERROR
	};
	
private:
	ErrorKind errorKind;
	string msg;
	bool m_hasErrorPage;
	bool m_isHTML;
	string m_errorPage;
	string preloaderCommand;
	map<string, string> annotations;
	
public:
	SpawnException(const string &message, ErrorKind errorKind = UNDEFINED_ERROR)
		: msg(message)
	{
		this->errorKind = errorKind;
		m_hasErrorPage = false;
		m_isHTML = false;
	}
	
	SpawnException(const string &message, const string &errorPage,
		bool isHTML = true, ErrorKind errorKind = UNDEFINED_ERROR)
		: msg(message), m_errorPage(errorPage)
	{
		assert(!isHTML
			|| errorKind == UNDEFINED_ERROR
			|| errorKind == PRELOADER_STARTUP_EXPLAINABLE_ERROR
			|| errorKind == APP_STARTUP_EXPLAINABLE_ERROR);
		this->errorKind = errorKind;
		m_hasErrorPage = true;
		m_isHTML = isHTML;
	}
	
	virtual ~SpawnException() throw() {}
	
	virtual const char *what() const throw() {
		return msg.c_str();
	}
	
	bool hasErrorPage() const {
		return m_hasErrorPage;
	}
	
	const string &getErrorPage() const {
		return m_errorPage;
	}
	
	bool isHTML() const {
		return m_isHTML;
	}
	
	ErrorKind getErrorKind() const {
		return errorKind;
	}
	
	SpawnException &setPreloaderCommand(const string &filename) {
		preloaderCommand = filename;
		return *this;
	}
	
	const string &getPreloaderCommand() const {
		return preloaderCommand;
	}

	void addAnnotations(const map<string, string> &annotations) {
		map<string, string>::const_iterator it, end = annotations.end();
		for (it = annotations.begin(); it != end; it++) {
			this->annotations[it->first] = it->second;
		}
	}

	const map<string, string> &getAnnotations() const {
		return annotations;
	}

	string operator[](const string &name) const {
		return get(name);
	}

	string get(const string &name) const {
		map<string, string>::const_iterator it = annotations.find(name);
		if (it == annotations.end()) {
			return string();
		} else {
			return it->second;
		}
	}
};

/**
 * Indicates that a Pool::get() or Pool::asyncGet() request was denied.
 * The request never reached a process. This could be because, before the
 * request could reach a process, the administrator detached the containing
 * SuperGroup. Or maybe the request sat in the queue for too long.
 */
class GetAbortedException: public oxt::tracable_exception {
private:
	string msg;

public:
	GetAbortedException(const string &message)
		: msg(message)
		{ }
	
	virtual ~GetAbortedException() throw() {}
	
	virtual const char *what() const throw() {
		return msg.c_str();
	}
};

/**
 * Indicates that a specified argument is incorrect or violates a requirement.
 *
 * @ingroup Exceptions
 */
class ArgumentException: public oxt::tracable_exception {
private:
	string msg;
public:
	ArgumentException(const string &message): msg(message) {}
	virtual ~ArgumentException() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

/*
 * @ingroup Exceptions
 */
class InvalidModeStringException: public ArgumentException {
public:
	InvalidModeStringException(const string &message): ArgumentException(message) {}
};

/**
 * A generic runtime exception.
 *
 * @ingroup Exceptions
 */
class RuntimeException: public oxt::tracable_exception {
private:
	string msg;
public:
	RuntimeException(const string &message): msg(message) {}
	virtual ~RuntimeException() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

/**
 * An exception indicating that some timeout expired.
 *
 * @ingroup Exceptions
 */
class TimeoutException: public oxt::tracable_exception {
private:
	string msg;
public:
	TimeoutException(const string &message): msg(message) {}
	virtual ~TimeoutException() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

/**
 * Represents some kind of security error.
 *
 * @ingroup Exceptions
 */
class SecurityException: public oxt::tracable_exception {
private:
	string msg;
public:
	SecurityException(const string &message): msg(message) {}
	virtual ~SecurityException() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

/**
 * @ingroup Exceptions
 */
class NonExistentUserException: public SecurityException {
public:
	NonExistentUserException(const string &message): SecurityException(message) {}
};

/**
 * @ingroup Exceptions
 */
class NonExistentGroupException: public SecurityException {
public:
	NonExistentGroupException(const string &message): SecurityException(message) {}
};

/**
 * The application pool is too busy and cannot fulfill a get() request.
 *
 * @ingroup Exceptions
 */
class BusyException: public oxt::tracable_exception {
private:
	string msg;
public:
	BusyException(const string &message): msg(message) {}
	virtual ~BusyException() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

/**
 * A parser detected a syntax error.
 *
 * @ingroup Exceptions
 */
class SyntaxError: public oxt::tracable_exception {
private:
	string msg;
public:
	SyntaxError(const string &message): msg(message) {}
	virtual ~SyntaxError() throw() {}
	virtual const char *what() const throw() { return msg.c_str(); }
};

} // namespace Passenger

#endif /* _PASSENGER_EXCEPTIONS_H_ */
