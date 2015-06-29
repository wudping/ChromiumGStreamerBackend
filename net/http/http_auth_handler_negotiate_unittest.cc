// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_info.h"
#include "net/http/mock_allow_url_security_manager.h"
#if defined(OS_WIN)
#include "net/http/mock_sspi_library_win.h"
#elif defined(OS_POSIX)
#include "net/http/mock_gssapi_library_posix.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

#if defined(OS_WIN)
typedef MockSSPILibrary MockAuthLibrary;
#elif defined(OS_POSIX)
typedef test::MockGSSAPILibrary MockAuthLibrary;
#endif

class HttpAuthHandlerNegotiateTest : public PlatformTest {
 public:
  void SetUp() override {
    auth_library_ = new MockAuthLibrary();
    resolver_.reset(new MockHostResolver());
    resolver_->rules()->AddIPLiteralRule("alias", "10.0.0.2",
                                           "canonical.example.com");

    url_security_manager_.reset(new MockAllowURLSecurityManager());
    factory_.reset(new HttpAuthHandlerNegotiate::Factory());
    factory_->set_url_security_manager(url_security_manager_.get());
    factory_->set_library(auth_library_);
    factory_->set_host_resolver(resolver_.get());
  }

  void SetupMocks(MockAuthLibrary* mock_library) {
#if defined(OS_WIN)
    security_package_.reset(new SecPkgInfoW);
    memset(security_package_.get(), 0x0, sizeof(SecPkgInfoW));
    security_package_->cbMaxToken = 1337;
    mock_library->ExpectQuerySecurityPackageInfo(
        L"Negotiate", SEC_E_OK, security_package_.get());
#elif defined(OS_POSIX)
    // Copied from an actual transaction!
    static const char kAuthResponse[] =
        "\x60\x82\x02\xCA\x06\x09\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x01"
        "\x00\x6E\x82\x02\xB9\x30\x82\x02\xB5\xA0\x03\x02\x01\x05\xA1\x03"
        "\x02\x01\x0E\xA2\x07\x03\x05\x00\x00\x00\x00\x00\xA3\x82\x01\xC1"
        "\x61\x82\x01\xBD\x30\x82\x01\xB9\xA0\x03\x02\x01\x05\xA1\x16\x1B"
        "\x14\x55\x4E\x49\x58\x2E\x43\x4F\x52\x50\x2E\x47\x4F\x4F\x47\x4C"
        "\x45\x2E\x43\x4F\x4D\xA2\x2C\x30\x2A\xA0\x03\x02\x01\x01\xA1\x23"
        "\x30\x21\x1B\x04\x68\x6F\x73\x74\x1B\x19\x6E\x69\x6E\x6A\x61\x2E"
        "\x63\x61\x6D\x2E\x63\x6F\x72\x70\x2E\x67\x6F\x6F\x67\x6C\x65\x2E"
        "\x63\x6F\x6D\xA3\x82\x01\x6A\x30\x82\x01\x66\xA0\x03\x02\x01\x10"
        "\xA1\x03\x02\x01\x01\xA2\x82\x01\x58\x04\x82\x01\x54\x2C\xB1\x2B"
        "\x0A\xA5\xFF\x6F\xEC\xDE\xB0\x19\x6E\x15\x20\x18\x0C\x42\xB3\x2C"
        "\x4B\xB0\x37\x02\xDE\xD3\x2F\xB4\xBF\xCA\xEC\x0E\xF9\xF3\x45\x6A"
        "\x43\xF3\x8D\x79\xBD\xCB\xCD\xB2\x2B\xB8\xFC\xD6\xB4\x7F\x09\x48"
        "\x14\xA7\x4F\xD2\xEE\xBC\x1B\x2F\x18\x3B\x81\x97\x7B\x28\xA4\xAF"
        "\xA8\xA3\x7A\x31\x1B\xFC\x97\xB6\xBA\x8A\x50\x50\xD7\x44\xB8\x30"
        "\xA4\x51\x4C\x3A\x95\x6C\xA1\xED\xE2\xEF\x17\xFE\xAB\xD2\xE4\x70"
        "\xDE\xEB\x7E\x86\x48\xC5\x3E\x19\x5B\x83\x17\xBB\x52\x26\xC0\xF3"
        "\x38\x0F\xB0\x8C\x72\xC9\xB0\x8B\x99\x96\x18\xE1\x9E\x67\x9D\xDC"
        "\xF5\x39\x80\x70\x35\x3F\x98\x72\x16\x44\xA2\xC0\x10\xAA\x70\xBD"
        "\x06\x6F\x83\xB1\xF4\x67\xA4\xBD\xDA\xF7\x79\x1D\x96\xB5\x7E\xF8"
        "\xC6\xCF\xB4\xD9\x51\xC9\xBB\xB4\x20\x3C\xDD\xB9\x2C\x38\xEA\x40"
        "\xFB\x02\x6C\xCB\x48\x71\xE8\xF4\x34\x5B\x63\x5D\x13\x57\xBD\xD1"
        "\x3D\xDE\xE8\x4A\x51\x6E\xBE\x4C\xF5\xA3\x84\xF7\x4C\x4E\x58\x04"
        "\xBE\xD1\xCC\x22\xA0\x43\xB0\x65\x99\x6A\xE0\x78\x0D\xFC\xE1\x42"
        "\xA9\x18\xCF\x55\x4D\x23\xBD\x5C\x0D\xB5\x48\x25\x47\xCC\x01\x54"
        "\x36\x4D\x0C\x6F\xAC\xCD\x33\x21\xC5\x63\x18\x91\x68\x96\xE9\xD1"
        "\xD8\x23\x1F\x21\xAE\x96\xA3\xBD\x27\xF7\x4B\xEF\x4C\x43\xFF\xF8"
        "\x22\x57\xCF\x68\x6C\x35\xD5\x21\x48\x5B\x5F\x8F\xA5\xB9\x6F\x99"
        "\xA6\xE0\x6E\xF0\xC5\x7C\x91\xC8\x0B\x8A\x4B\x4E\x80\x59\x02\xE9"
        "\xE8\x3F\x87\x04\xA6\xD1\xCA\x26\x3C\xF0\xDA\x57\xFA\xE6\xAF\x25"
        "\x43\x34\xE1\xA4\x06\x1A\x1C\xF4\xF5\x21\x9C\x00\x98\xDD\xF0\xB4"
        "\x8E\xA4\x81\xDA\x30\x81\xD7\xA0\x03\x02\x01\x10\xA2\x81\xCF\x04"
        "\x81\xCC\x20\x39\x34\x60\x19\xF9\x4C\x26\x36\x46\x99\x7A\xFD\x2B"
        "\x50\x8B\x2D\x47\x72\x38\x20\x43\x0E\x6E\x28\xB3\xA7\x4F\x26\xF1"
        "\xF1\x7B\x02\x63\x58\x5A\x7F\xC8\xD0\x6E\xF5\xD1\xDA\x28\x43\x1B"
        "\x6D\x9F\x59\x64\xDE\x90\xEA\x6C\x8C\xA9\x1B\x1E\x92\x29\x24\x23"
        "\x2C\xE3\xEA\x64\xEF\x91\xA5\x4E\x94\xE1\xDC\x56\x3A\xAF\xD5\xBC"
        "\xC9\xD3\x9B\x6B\x1F\xBE\x40\xE5\x40\xFF\x5E\x21\xEA\xCE\xFC\xD5"
        "\xB0\xE5\xBA\x10\x94\xAE\x16\x54\xFC\xEB\xAB\xF1\xD4\x20\x31\xCC"
        "\x26\xFE\xBE\xFE\x22\xB6\x9B\x1A\xE5\x55\x2C\x93\xB7\x3B\xD6\x4C"
        "\x35\x35\xC1\x59\x61\xD4\x1F\x2E\x4C\xE1\x72\x8F\x71\x4B\x0C\x39"
        "\x80\x79\xFA\xCD\xEA\x71\x1B\xAE\x35\x41\xED\xF9\x65\x0C\x59\xF8"
        "\xE1\x27\xDA\xD6\xD1\x20\x32\xCD\xBF\xD1\xEF\xE2\xED\xAD\x5D\xA7"
        "\x69\xE3\x55\xF9\x30\xD3\xD4\x08\xC8\xCA\x62\xF8\x64\xEC\x9B\x92"
        "\x1A\xF1\x03\x2E\xCC\xDC\xEB\x17\xDE\x09\xAC\xA9\x58\x86";
    test::GssContextMockImpl context1(
        "localhost",                         // Source name
        "example.com",                       // Target name
        23,                                  // Lifetime
        *CHROME_GSS_SPNEGO_MECH_OID_DESC,    // Mechanism
        0,                                   // Context flags
        1,                                   // Locally initiated
        0);                                  // Open
    test::GssContextMockImpl context2(
        "localhost",                         // Source name
        "example.com",                       // Target name
        23,                                  // Lifetime
        *CHROME_GSS_SPNEGO_MECH_OID_DESC,    // Mechanism
        0,                                   // Context flags
        1,                                   // Locally initiated
        1);                                  // Open
    test::MockGSSAPILibrary::SecurityContextQuery queries[] = {
    test::MockGSSAPILibrary::SecurityContextQuery(
        "Negotiate",                    // Package name
        GSS_S_CONTINUE_NEEDED,          // Major response code
        0,                              // Minor response code
        context1,                       // Context
        NULL,                           // Expected input token
        kAuthResponse),                 // Output token
    test::MockGSSAPILibrary::SecurityContextQuery(
        "Negotiate",                    // Package name
        GSS_S_COMPLETE,                 // Major response code
        0,                              // Minor response code
        context2,                       // Context
        kAuthResponse,                  // Expected input token
        kAuthResponse)                  // Output token
    };

    for (size_t i = 0; i < arraysize(queries); ++i) {
      mock_library->ExpectSecurityContext(queries[i].expected_package,
                                          queries[i].response_code,
                                          queries[i].minor_response_code,
                                          queries[i].context_info,
                                          queries[i].expected_input_token,
                                          queries[i].output_token);
    }
#endif  // defined(OS_POSIX)
  }

#if defined(OS_POSIX)
  void SetupErrorMocks(MockAuthLibrary* mock_library,
                       int major_status,
                       int minor_status) {
    const gss_OID_desc kDefaultMech = { 0, NULL };
    test::GssContextMockImpl context(
        "localhost",                    // Source name
        "example.com",                  // Target name
        0,                              // Lifetime
        kDefaultMech,                   // Mechanism
        0,                              // Context flags
        1,                              // Locally initiated
        0);                             // Open
    test::MockGSSAPILibrary::SecurityContextQuery query(
        "Negotiate",                    // Package name
        major_status,                   // Major response code
        minor_status,                   // Minor response code
        context,                        // Context
        NULL,                           // Expected input token
        NULL);                          // Output token

    mock_library->ExpectSecurityContext(query.expected_package,
                                        query.response_code,
                                        query.minor_response_code,
                                        query.context_info,
                                        query.expected_input_token,
                                        query.output_token);
  }

#endif  // defined(OS_POSIX)

  int CreateHandler(bool disable_cname_lookup, bool use_port,
                     bool synchronous_resolve_mode,
                     const std::string& url_string,
                     scoped_ptr<HttpAuthHandlerNegotiate>* handler) {
    factory_->set_disable_cname_lookup(disable_cname_lookup);
    factory_->set_use_port(use_port);
    resolver_->set_synchronous_mode(synchronous_resolve_mode);
    GURL gurl(url_string);

    // Note: This is a little tricky because CreateAuthHandlerFromString
    // expects a scoped_ptr<HttpAuthHandler>* rather than a
    // scoped_ptr<HttpAuthHandlerNegotiate>*. This needs to do the cast
    // after creating the handler, and make sure that generic_handler
    // no longer holds on to the HttpAuthHandlerNegotiate object.
    scoped_ptr<HttpAuthHandler> generic_handler;
    int rv = factory_->CreateAuthHandlerFromString("Negotiate",
                                                   HttpAuth::AUTH_SERVER,
                                                   gurl,
                                                   BoundNetLog(),
                                                   &generic_handler);
    if (rv != OK)
      return rv;
    HttpAuthHandlerNegotiate* negotiate_handler =
        static_cast<HttpAuthHandlerNegotiate*>(generic_handler.release());
    handler->reset(negotiate_handler);
    return rv;
  }

  MockAuthLibrary* AuthLibrary() { return auth_library_; }

 private:
#if defined(OS_WIN)
  scoped_ptr<SecPkgInfoW> security_package_;
#endif
  // |auth_library_| is passed to |factory_|, which assumes ownership of it.
  MockAuthLibrary* auth_library_;
  scoped_ptr<MockHostResolver> resolver_;
  scoped_ptr<URLSecurityManager> url_security_manager_;
  scoped_ptr<HttpAuthHandlerNegotiate::Factory> factory_;
};

TEST_F(HttpAuthHandlerNegotiateTest, DisableCname) {
  SetupMocks(AuthLibrary());
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      true, false, true, "http://alias:500", &auth_handler));

  ASSERT_TRUE(auth_handler.get() != NULL);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(NULL, &request_info,
                                                callback.callback(), &token));
#if defined(OS_WIN)
  EXPECT_EQ("HTTP/alias", auth_handler->spn());
#elif defined(OS_POSIX)
  EXPECT_EQ("HTTP@alias", auth_handler->spn());
#endif
}

TEST_F(HttpAuthHandlerNegotiateTest, DisableCnameStandardPort) {
  SetupMocks(AuthLibrary());
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      true, true, true, "http://alias:80", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != NULL);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(NULL, &request_info,
                                                callback.callback(), &token));
#if defined(OS_WIN)
  EXPECT_EQ("HTTP/alias", auth_handler->spn());
#elif defined(OS_POSIX)
  EXPECT_EQ("HTTP@alias", auth_handler->spn());
#endif
}

TEST_F(HttpAuthHandlerNegotiateTest, DisableCnameNonstandardPort) {
  SetupMocks(AuthLibrary());
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      true, true, true, "http://alias:500", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != NULL);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(NULL, &request_info,
                                                callback.callback(), &token));
#if defined(OS_WIN)
  EXPECT_EQ("HTTP/alias:500", auth_handler->spn());
#elif defined(OS_POSIX)
  EXPECT_EQ("HTTP@alias:500", auth_handler->spn());
#endif
}

TEST_F(HttpAuthHandlerNegotiateTest, CnameSync) {
  SetupMocks(AuthLibrary());
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      false, false, true, "http://alias:500", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != NULL);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(NULL, &request_info,
                                                callback.callback(), &token));
#if defined(OS_WIN)
  EXPECT_EQ("HTTP/canonical.example.com", auth_handler->spn());
#elif defined(OS_POSIX)
  EXPECT_EQ("HTTP@canonical.example.com", auth_handler->spn());
#endif
}

TEST_F(HttpAuthHandlerNegotiateTest, CnameAsync) {
  SetupMocks(AuthLibrary());
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      false, false, false, "http://alias:500", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != NULL);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(ERR_IO_PENDING, auth_handler->GenerateAuthToken(
      NULL, &request_info, callback.callback(), &token));
  EXPECT_EQ(OK, callback.WaitForResult());
#if defined(OS_WIN)
  EXPECT_EQ("HTTP/canonical.example.com", auth_handler->spn());
#elif defined(OS_POSIX)
  EXPECT_EQ("HTTP@canonical.example.com", auth_handler->spn());
#endif
}

#if defined(OS_POSIX)

// This test is only for GSSAPI, as we can't use explicit credentials with
// that library.
TEST_F(HttpAuthHandlerNegotiateTest, ServerNotInKerberosDatabase) {
  SetupErrorMocks(AuthLibrary(), GSS_S_FAILURE, 0x96C73A07);  // No server
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      false, false, false, "http://alias:500", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != NULL);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(ERR_IO_PENDING, auth_handler->GenerateAuthToken(
      NULL, &request_info, callback.callback(), &token));
  EXPECT_EQ(ERR_MISSING_AUTH_CREDENTIALS, callback.WaitForResult());
}

// This test is only for GSSAPI, as we can't use explicit credentials with
// that library.
TEST_F(HttpAuthHandlerNegotiateTest, NoKerberosCredentials) {
  SetupErrorMocks(AuthLibrary(), GSS_S_FAILURE, 0x96C73AC3);  // No credentials
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  EXPECT_EQ(OK, CreateHandler(
      false, false, false, "http://alias:500", &auth_handler));
  ASSERT_TRUE(auth_handler.get() != NULL);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  EXPECT_EQ(ERR_IO_PENDING, auth_handler->GenerateAuthToken(
      NULL, &request_info, callback.callback(), &token));
  EXPECT_EQ(ERR_MISSING_AUTH_CREDENTIALS, callback.WaitForResult());
}

#if defined(DLOPEN_KERBEROS)
TEST_F(HttpAuthHandlerNegotiateTest, MissingGSSAPI) {
  scoped_ptr<HostResolver> host_resolver(new MockHostResolver());
  MockAllowURLSecurityManager url_security_manager;
  scoped_ptr<HttpAuthHandlerNegotiate::Factory> negotiate_factory(
      new HttpAuthHandlerNegotiate::Factory());
  negotiate_factory->set_host_resolver(host_resolver.get());
  negotiate_factory->set_url_security_manager(&url_security_manager);
  negotiate_factory->set_library(
      new GSSAPISharedLibrary("/this/library/does/not/exist"));

  GURL gurl("http://www.example.com");
  scoped_ptr<HttpAuthHandler> generic_handler;
  int rv = negotiate_factory->CreateAuthHandlerFromString(
      "Negotiate",
      HttpAuth::AUTH_SERVER,
      gurl,
      BoundNetLog(),
      &generic_handler);
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME, rv);
  EXPECT_TRUE(generic_handler.get() == NULL);
}
#endif  // defined(DLOPEN_KERBEROS)

#endif  // defined(OS_POSIX)

}  // namespace net
