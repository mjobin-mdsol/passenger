/*
 *  Phusion Passenger - https://www.phusionpassenger.com/
 *  Copyright (c) 2014 Phusion
 *
 *  "Phusion Passenger" is a trademark of Hongli Lai & Ninh Bui.
 *
 *  See LICENSE file for license information.
 */
#ifndef _PASSENGER_APP_RESPONSE_H_
#define _PASSENGER_APP_RESPONSE_H_

#include <Utils/sysqueue.h>
#include <boost/cstdint.hpp>
#include <boost/atomic.hpp>
#include <ServerKit/http_parser.h>
#include <ServerKit/Hooks.h>
#include <ServerKit/Client.h>
#include <ServerKit/HeaderTable.h>
#include <ServerKit/HttpHeaderParserState.h>
#include <ServerKit/HttpChunkedBodyParserState.h>
#include <MemoryKit/palloc.h>
#include <DataStructures/LString.h>

namespace Passenger {


class HttpHeaderParser;

class AppResponse {
public:
	enum HttpState {
		/** The headers are still being parsed. */
		PARSING_HEADERS,
		/** Internal state used by the parser. Users should never see this state. */
		PARSED_HEADERS,
		/** The headers have been parsed, and there is no body. */
		COMPLETE,
		/** The headers have been parsed, and we are now receiving/parsing the body,
		 * whose length is specified by Content-Length. */
		PARSING_BODY_WITH_LENGTH,
		/** The headers have been parsed, and we are now receiving/parsing the body,
		 * which has the chunked transfer-encoding. */
		PARSING_CHUNKED_BODY,
		/** The headers have been parsed, and we are now receiving/parsing the body,
		 * which ends when EOF is encountered on the app socket. */
		PARSING_BODY_UNTIL_EOF,
		/** The headers have been parsed, and the connection has been upgraded. */
		UPGRADED,
		/** A 100-Continue status line has been encountered. */
		ONEHUNDRED_CONTINUE,
		/** An error occurred. */
		ERROR
	};

	// Enum values are deliberately chosen so that hasRequestBody() can be branchless.
	enum BodyType {
		/** The message has no body. */
		RBT_NO_BODY = 0,
		/** The connection has been upgraded. */
		RBT_UPGRADE = 1,
		/** The message body's size is determined by the Content-Length header. */
		RBT_CONTENT_LENGTH = 2,
		/** The message body's size is determined by the chunked Transfer-Encoding. */
		RBT_CHUNKED = 4,
		/** The message body's size is equal to the stream's size. */
		RBT_UNTIL_EOF = 8
	};

	boost::uint8_t httpMajor;
	boost::uint8_t httpMinor;
	HttpState httpState: 5;
	bool wantKeepAlive: 1;
	bool oneHundredContinueSent: 1;
	bool hasDateHeader: 1;
	BodyType bodyType;

	boost::uint16_t statusCode;

	union {
		// If httpState == PARSING_HEADERS
		ServerKit::HttpHeaderParserState *headerParser;
		// If httpState == PARSING_CHUNKED_BODY
		ServerKit::HttpChunkedBodyParserState chunkedBodyParser;
	} parserState;
	ServerKit::HeaderTable headers;
	ServerKit::HeaderTable secureHeaders;

	union {
		/** Length of the message body. Only use when httpState != ERROR. */
		union {
			// If bodyType == RBT_CONTENT_LENGTH. Guaranteed to be > 0.
			boost::uint64_t contentLength;
			// If bodyType == RBT_CHUNKED
			bool endChunkReached;
			// If bodyType == PARSING_BODY_UNTIL_EOF
			bool endReached;
		} bodyInfo;

		/** If a request parsing error occurred, the error code is stored here.
		 * Only use if httpState == ERROR.
		 */
		int parseError;
	} aux;
	boost::uint64_t bodyAlreadyRead;


	AppResponse()
		: headers(16),
		  secureHeaders(0),
		  bodyAlreadyRead(0)
	{
		parserState.headerParser = NULL;
		aux.bodyInfo.contentLength = 0; // Sets the entire union to 0.
	}

	const char *getHttpStateString() const {
		switch (httpState) {
		case PARSING_HEADERS:
			return "PARSING_HEADERS";
		case PARSED_HEADERS:
			return "PARSED_HEADERS";
		case COMPLETE:
			return "COMPLETE";
		case PARSING_BODY_WITH_LENGTH:
			return "PARSING_BODY_WITH_LENGTH";
		case PARSING_CHUNKED_BODY:
			return "PARSING_CHUNKED_BODY";
		case PARSING_BODY_UNTIL_EOF:
			return "PARSING_BODY_UNTIL_EOF";
		case UPGRADED:
			return "UPGRADED";
		case ONEHUNDRED_CONTINUE:
			return "ONEHUNDRED_CONTINUE";
		case ERROR:
			return "ERROR";
		}
	}

	const char *getBodyTypeString() const {
		switch (bodyType) {
		case RBT_NO_BODY:
			return "NO_BODY";
		case RBT_UPGRADE:
			return "UPGRADE";
		case RBT_CONTENT_LENGTH:
			return "CONTENT_LENGTH";
		case RBT_UNTIL_EOF:
			return "RBT_UNTIL_EOF";
		case RBT_CHUNKED:
			return "CHUNKED";
		}
	}

	bool bodyFullyRead() const {
		switch (bodyType) {
		case RBT_NO_BODY:
			return true;
		case RBT_UPGRADE:
			return false;
		case RBT_CONTENT_LENGTH:
			return bodyAlreadyRead >= aux.bodyInfo.contentLength;
		case RBT_CHUNKED:
			return aux.bodyInfo.endChunkReached;
		case RBT_UNTIL_EOF:
			return aux.bodyInfo.endReached;
		}
	}

	bool hasBody() const {
		return bodyType & (RBT_CONTENT_LENGTH | RBT_CHUNKED | RBT_UNTIL_EOF);
	}

	bool canKeepAlive() const {
		return wantKeepAlive && bodyFullyRead();
	}
};


} // namespace Passenger

#endif /* _PASSENGER_APP_RESPONSE_H_ */