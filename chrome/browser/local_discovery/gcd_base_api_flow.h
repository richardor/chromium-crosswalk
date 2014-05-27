// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_GCD_BASE_API_FLOW_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_GCD_BASE_API_FLOW_H_

#include <string>

#include "chrome/browser/local_discovery/privet_constants.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace local_discovery {

// API call flow for communicating with cloud print.
class GCDBaseApiFlow : public net::URLFetcherDelegate,
                       public OAuth2TokenService::Consumer {
 public:
  // TODO(noamsml): Better error model for this class.
  enum Status {
    SUCCESS,
    ERROR_TOKEN,
    ERROR_NETWORK,
    ERROR_HTTP_CODE,
    ERROR_FROM_SERVER,
    ERROR_MALFORMED_RESPONSE
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnGCDAPIFlowError(GCDBaseApiFlow* flow, Status status) = 0;

    virtual void OnGCDAPIFlowComplete(GCDBaseApiFlow* flow,
                                      const base::DictionaryValue* value) = 0;

    virtual GURL GetURL() = 0;

    virtual std::string GetOAuthScope();

    virtual net::URLFetcher::RequestType GetRequestType();

    virtual std::vector<std::string> GetExtraRequestHeaders();

    // If there is no data, set upload_type and upload_data to ""
    virtual void GetUploadData(std::string* upload_type,
                               std::string* upload_data);
  };

  // Create an OAuth2-based confirmation.
  GCDBaseApiFlow(net::URLRequestContextGetter* request_context,
                 OAuth2TokenService* token_service,
                 const std::string& account_id,
                 Delegate* delegate);

  virtual ~GCDBaseApiFlow();

  void Start();

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // OAuth2TokenService::Consumer implementation:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  void CreateRequest(const GURL& url);

  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_ptr<OAuth2TokenService::Request> oauth_request_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  OAuth2TokenService* token_service_;
  std::string account_id_;
  Delegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(GCDBaseApiFlow);
};

class CloudPrintApiFlowDelegate : public GCDBaseApiFlow::Delegate {
 public:
  CloudPrintApiFlowDelegate();
  virtual ~CloudPrintApiFlowDelegate();

  // GCDBaseApiFlow::Delegate implementation
  virtual std::string GetOAuthScope() OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintApiFlowDelegate);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_GCD_BASE_API_FLOW_H_
