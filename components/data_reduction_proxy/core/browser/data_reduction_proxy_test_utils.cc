// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"

namespace {

const char kTestKey[] = "test-key";

const net::BackoffEntry::Policy kTestBackoffPolicy = {
    0,               // num_errors_to_ignore
    10 * 1000,       // initial_delay_ms
    2,               // multiply_factor
    0,               // jitter_factor
    30 * 60 * 1000,  // maximum_backoff_ms
    -1,              // entry_lifetime_ms
    true,            // always_use_initial_delay
};

}  // namespace

namespace data_reduction_proxy {

TestDataReductionProxyRequestOptions::TestDataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : DataReductionProxyRequestOptions(client, version, config, task_runner) {
}

std::string TestDataReductionProxyRequestOptions::GetDefaultKey() const {
  return kTestKey;
}

base::Time TestDataReductionProxyRequestOptions::Now() const {
  return base::Time::UnixEpoch() + now_offset_;
}

void TestDataReductionProxyRequestOptions::RandBytes(void* output,
                                                     size_t length) const {
  char* c = static_cast<char*>(output);
  for (size_t i = 0; i < length; ++i) {
    c[i] = 'a';
  }
}

// Time after the unix epoch that Now() reports.
void TestDataReductionProxyRequestOptions::set_offset(
    const base::TimeDelta& now_offset) {
  now_offset_ = now_offset;
}

MockDataReductionProxyRequestOptions::MockDataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : DataReductionProxyRequestOptions(client, version, config, task_runner) {
}

MockDataReductionProxyRequestOptions::~MockDataReductionProxyRequestOptions() {
}

TestDataReductionProxyConfigServiceClient::
    TestDataReductionProxyConfigServiceClient(
        scoped_ptr<DataReductionProxyParams> params,
        DataReductionProxyRequestOptions* request_options,
        DataReductionProxyMutableConfigValues* config_values,
        DataReductionProxyConfig* config,
        scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : DataReductionProxyConfigServiceClient(params.Pass(),
                                            kTestBackoffPolicy,
                                            request_options,
                                            config_values,
                                            config,
                                            io_task_runner),
      tick_clock_(base::Time::UnixEpoch()),
      test_backoff_entry_(&kTestBackoffPolicy, &tick_clock_) {
}

TestDataReductionProxyConfigServiceClient::
    ~TestDataReductionProxyConfigServiceClient() {
}

void TestDataReductionProxyConfigServiceClient::SetNow(const base::Time& time) {
  tick_clock_.SetTime(time);
}

void TestDataReductionProxyConfigServiceClient::SetCustomReleaseTime(
    const base::TimeTicks& release_time) {
  test_backoff_entry_.SetCustomReleaseTime(release_time);
}

base::TimeDelta TestDataReductionProxyConfigServiceClient::GetDelay() const {
  return config_refresh_timer_.GetCurrentDelay();
}

base::Time TestDataReductionProxyConfigServiceClient::Now() const {
  return tick_clock_.Now();
}

net::BackoffEntry*
TestDataReductionProxyConfigServiceClient::GetBackoffEntry() {
  return &test_backoff_entry_;
}

TestDataReductionProxyConfigServiceClient::TestTickClock::TestTickClock(
    const base::Time& initial_time)
    : time_(initial_time) {
}

base::TimeTicks
TestDataReductionProxyConfigServiceClient::TestTickClock::NowTicks() const {
  return base::TimeTicks::UnixEpoch() + (time_ - base::Time::UnixEpoch());
}

base::Time
TestDataReductionProxyConfigServiceClient::TestTickClock::Now() const {
  return time_;
}

void TestDataReductionProxyConfigServiceClient::TestTickClock::SetTime(
    const base::Time& time) {
  time_ = time;
}

TestDataReductionProxyConfigServiceClient::TestBackoffEntry::TestBackoffEntry(
    const net::BackoffEntry::Policy* const policy,
    const TestTickClock* tick_clock)
    : net::BackoffEntry(policy),
      tick_clock_(tick_clock) {
}

base::TimeTicks
TestDataReductionProxyConfigServiceClient::TestBackoffEntry::ImplGetTimeNow()
    const {
  return tick_clock_->NowTicks();
}

MockDataReductionProxyService::MockDataReductionProxyService(
    scoped_ptr<DataReductionProxyCompressionStats> compression_stats,
    DataReductionProxySettings* settings,
    net::URLRequestContextGetter* request_context)
    : DataReductionProxyService(
        compression_stats.Pass(), settings, request_context) {
}

MockDataReductionProxyService::~MockDataReductionProxyService() {
}

TestDataReductionProxyIOData::TestDataReductionProxyIOData(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_ptr<DataReductionProxyConfig> config,
    scoped_ptr<DataReductionProxyEventStore> event_store,
    scoped_ptr<DataReductionProxyRequestOptions> request_options,
    scoped_ptr<DataReductionProxyConfigurator> configurator,
    scoped_ptr<DataReductionProxyConfigServiceClient> config_client)
    : DataReductionProxyIOData() {
  io_task_runner_ = task_runner;
  ui_task_runner_ = task_runner;
  config_ = config.Pass();
  event_store_ = event_store.Pass();
  request_options_ = request_options.Pass();
  configurator_ = configurator.Pass();
  config_client_ = config_client.Pass();
  io_task_runner_ = task_runner;
  ui_task_runner_ = task_runner;
}

TestDataReductionProxyIOData::~TestDataReductionProxyIOData() {
  shutdown_on_ui_ = true;
}

DataReductionProxyTestContext::Builder::Builder()
    : params_flags_(0),
      params_definitions_(0),
      client_(Client::UNKNOWN),
      request_context_(nullptr),
      mock_socket_factory_(nullptr),
      use_mock_config_(false),
      use_test_configurator_(false),
      use_mock_service_(false),
      use_mock_request_options_(false),
      use_config_client_(false),
      use_test_config_client_(false),
      skip_settings_initialization_(false) {
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithParamsFlags(int params_flags) {
  params_flags_ = params_flags;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithParamsDefinitions(
    unsigned int params_definitions) {
  params_definitions_ = params_definitions;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithURLRequestContext(
    net::URLRequestContext* request_context) {
  request_context_ = request_context;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockClientSocketFactory(
    net::MockClientSocketFactory* mock_socket_factory) {
  mock_socket_factory_ = mock_socket_factory;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithClient(Client client) {
  client_ = client;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockConfig() {
  use_mock_config_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithTestConfigurator() {
  use_test_configurator_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockDataReductionProxyService() {
  use_mock_service_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockRequestOptions() {
  use_mock_request_options_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithConfigClient() {
  use_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithTestConfigClient() {
  use_config_client_ = true;
  use_test_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::SkipSettingsInitialization() {
  skip_settings_initialization_ = true;
  return *this;
}

scoped_ptr<DataReductionProxyTestContext>
DataReductionProxyTestContext::Builder::Build() {
  // Check for invalid builder combinations.
  DCHECK(!(use_mock_config_ && use_config_client_));
  scoped_ptr<base::MessageLoopForIO> loop(new base::MessageLoopForIO());

  unsigned int test_context_flags = 0;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::MessageLoopProxy::current();
  scoped_refptr<net::URLRequestContextGetter> request_context_getter;
  scoped_ptr<TestingPrefServiceSimple> pref_service(
      new TestingPrefServiceSimple());
  scoped_ptr<net::CapturingNetLog> net_log(new net::CapturingNetLog());
  if (request_context_) {
    request_context_getter = new net::TrivialURLRequestContextGetter(
        request_context_, task_runner);
  } else {
    scoped_ptr<net::TestURLRequestContext> test_request_context(
        new net::TestURLRequestContext(true));
    if (mock_socket_factory_)
      test_request_context->set_client_socket_factory(mock_socket_factory_);
    test_request_context->Init();
    request_context_getter = new net::TestURLRequestContextGetter(
        task_runner, test_request_context.Pass());
  }

  scoped_ptr<DataReductionProxyEventStore> event_store(
      new DataReductionProxyEventStore(task_runner));
  scoped_ptr<DataReductionProxyConfigurator> configurator;
  if (use_test_configurator_) {
    test_context_flags |= USE_TEST_CONFIGURATOR;
    configurator.reset(new TestDataReductionProxyConfigurator(
        task_runner, net_log.get(), event_store.get()));
  } else {
    configurator.reset(new DataReductionProxyConfigurator(
        task_runner, net_log.get(), event_store.get()));
  }

  scoped_ptr<TestDataReductionProxyConfig> config;
  scoped_ptr<DataReductionProxyConfigServiceClient> config_client;
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
  scoped_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams(params_flags_, params_definitions_));
  TestDataReductionProxyParams* raw_params = params.get();
  if (use_config_client_) {
    test_context_flags |= USE_CONFIG_CLIENT;
    scoped_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        DataReductionProxyMutableConfigValues::CreateFromParams(task_runner,
                                                                params.get());
    raw_mutable_config = mutable_config.get();
    config.reset(new TestDataReductionProxyConfig(
        mutable_config.Pass(), task_runner, net_log.get(), configurator.get(),
        event_store.get()));
  } else if (use_mock_config_) {
    test_context_flags |= USE_MOCK_CONFIG;
    config.reset(new MockDataReductionProxyConfig(
        params.Pass(), task_runner, net_log.get(), configurator.get(),
        event_store.get()));
  } else {
    config.reset(new TestDataReductionProxyConfig(
        params.Pass(), task_runner, net_log.get(), configurator.get(),
        event_store.get()));
  }

  scoped_ptr<DataReductionProxyRequestOptions> request_options;
  if (use_mock_request_options_) {
    test_context_flags |= USE_MOCK_REQUEST_OPTIONS;
    request_options.reset(new MockDataReductionProxyRequestOptions(
        client_, std::string(), config.get(), task_runner));
  } else {
    request_options.reset(new DataReductionProxyRequestOptions(
        client_, config.get(), task_runner));
  }

  if (use_test_config_client_) {
    test_context_flags |= USE_TEST_CONFIG_CLIENT;
    config_client.reset(new TestDataReductionProxyConfigServiceClient(
        params.Pass(), request_options.get(), raw_mutable_config, config.get(),
        task_runner));
  } else if (use_config_client_) {
    config_client.reset(new DataReductionProxyConfigServiceClient(
        params.Pass(), GetBackoffPolicy(), request_options.get(),
        raw_mutable_config, config.get(), task_runner));
  }

  scoped_ptr<DataReductionProxySettings> settings(
      new DataReductionProxySettings());
  if (skip_settings_initialization_)
    test_context_flags |= SKIP_SETTINGS_INITIALIZATION;

  if (use_mock_service_)
    test_context_flags |= USE_MOCK_SERVICE;

  RegisterSimpleProfilePrefs(pref_service->registry());

  scoped_ptr<TestDataReductionProxyIOData> io_data(
      new TestDataReductionProxyIOData(
          task_runner, config.Pass(), event_store.Pass(),
          request_options.Pass(), configurator.Pass(), config_client.Pass()));
  io_data->InitOnUIThread(pref_service.get());

  scoped_ptr<DataReductionProxyTestContext> test_context(
      new DataReductionProxyTestContext(
          loop.Pass(), task_runner, pref_service.Pass(), net_log.Pass(),
          request_context_getter, mock_socket_factory_, io_data.Pass(),
          settings.Pass(), raw_params, test_context_flags));

  if (!skip_settings_initialization_)
    test_context->InitSettingsWithoutCheck();

  return test_context.Pass();
}

DataReductionProxyTestContext::DataReductionProxyTestContext(
    scoped_ptr<base::MessageLoop> loop,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_ptr<TestingPrefServiceSimple> simple_pref_service,
    scoped_ptr<net::CapturingNetLog> net_log,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    net::MockClientSocketFactory* mock_socket_factory,
    scoped_ptr<TestDataReductionProxyIOData> io_data,
    scoped_ptr<DataReductionProxySettings> settings,
    TestDataReductionProxyParams* params,
    unsigned int test_context_flags)
    : test_context_flags_(test_context_flags),
      loop_(loop.Pass()),
      task_runner_(task_runner),
      simple_pref_service_(simple_pref_service.Pass()),
      net_log_(net_log.Pass()),
      request_context_getter_(request_context_getter),
      mock_socket_factory_(mock_socket_factory),
      io_data_(io_data.Pass()),
      settings_(settings.Pass()),
      params_(params) {
}

DataReductionProxyTestContext::~DataReductionProxyTestContext() {
}

void DataReductionProxyTestContext::RunUntilIdle() {
  base::MessageLoop::current()->RunUntilIdle();
}

void DataReductionProxyTestContext::InitSettings() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  InitSettingsWithoutCheck();
}

void DataReductionProxyTestContext::InitSettingsWithoutCheck() {
  settings_->InitDataReductionProxySettings(
      simple_pref_service_.get(), io_data_.get(),
      CreateDataReductionProxyServiceInternal());
  io_data_->SetDataReductionProxyService(
      settings_->data_reduction_proxy_service()->GetWeakPtr());
}

scoped_ptr<DataReductionProxyService>
DataReductionProxyTestContext::CreateDataReductionProxyService() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  return CreateDataReductionProxyServiceInternal();
}

scoped_ptr<DataReductionProxyService>
DataReductionProxyTestContext::CreateDataReductionProxyServiceInternal() {
  scoped_ptr<DataReductionProxyCompressionStats> compression_stats =
      make_scoped_ptr(new DataReductionProxyCompressionStats(
          simple_pref_service_.get(), task_runner_, base::TimeDelta()));

  if (test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_SERVICE) {
    return make_scoped_ptr(new MockDataReductionProxyService(
        compression_stats.Pass(), settings_.get(),
        request_context_getter_.get()));
  } else {
    return make_scoped_ptr(
        new DataReductionProxyService(compression_stats.Pass(), settings_.get(),
                                      request_context_getter_.get()));
  }
}

void DataReductionProxyTestContext::AttachToURLRequestContext(
      net::URLRequestContextStorage* request_context_storage) const {
  DCHECK(request_context_storage);

  // |request_context_storage| takes ownership of the network delegate.
  request_context_storage->set_network_delegate(
      io_data()->CreateNetworkDelegate(
          scoped_ptr<net::NetworkDelegate>(new net::TestNetworkDelegate()),
          true).release());

  // |request_context_storage| takes ownership of the job factory.
  request_context_storage->set_job_factory(
      new net::URLRequestInterceptingJobFactory(
          scoped_ptr<net::URLRequestJobFactory>(
              new net::URLRequestJobFactoryImpl()),
          io_data()->CreateInterceptor().Pass()));
}

void DataReductionProxyTestContext::
    EnableDataReductionProxyWithSecureProxyCheckSuccess() {
  DCHECK(mock_socket_factory_);
  // This won't actually update the proxy config when using a test configurator.
  DCHECK(!(test_context_flags_ &
           DataReductionProxyTestContext::USE_TEST_CONFIGURATOR));
  // |settings_| needs to have been initialized, since a
  // |DataReductionProxyService| is needed in order to issue the secure proxy
  // check.
  DCHECK(data_reduction_proxy_service());

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check.
  net::MockRead mock_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead("OK"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      mock_reads, arraysize(mock_reads), nullptr, 0);
  mock_socket_factory_->AddSocketDataProvider(&socket_data_provider);

  // Set the pref to cause the secure proxy check to be issued.
  pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled, true);
  RunUntilIdle();
}

TestDataReductionProxyConfigurator*
DataReductionProxyTestContext::test_configurator() const {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_TEST_CONFIGURATOR);
  return reinterpret_cast<TestDataReductionProxyConfigurator*>(
      io_data_->configurator());
}

MockDataReductionProxyConfig* DataReductionProxyTestContext::mock_config()
    const {
  DCHECK(test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_CONFIG);
  return reinterpret_cast<MockDataReductionProxyConfig*>(io_data_->config());
}

DataReductionProxyService*
DataReductionProxyTestContext::data_reduction_proxy_service() const {
  return settings_->data_reduction_proxy_service();
}

MockDataReductionProxyService*
DataReductionProxyTestContext::mock_data_reduction_proxy_service()
    const {
  DCHECK(!(test_context_flags_ &
           DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION));
  DCHECK(test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_SERVICE);
  return reinterpret_cast<MockDataReductionProxyService*>(
      data_reduction_proxy_service());
}

MockDataReductionProxyRequestOptions*
DataReductionProxyTestContext::mock_request_options() const {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_MOCK_REQUEST_OPTIONS);
  return reinterpret_cast<MockDataReductionProxyRequestOptions*>(
      io_data_->request_options());
}

TestDataReductionProxyConfig* DataReductionProxyTestContext::config() const {
  return reinterpret_cast<TestDataReductionProxyConfig*>(io_data_->config());
}

DataReductionProxyMutableConfigValues*
DataReductionProxyTestContext::mutable_config_values() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_CONFIG_CLIENT);
  return reinterpret_cast<DataReductionProxyMutableConfigValues*>(
      config()->config_values());
}

TestDataReductionProxyConfigServiceClient*
DataReductionProxyTestContext::test_config_client() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_TEST_CONFIG_CLIENT);
  return reinterpret_cast<TestDataReductionProxyConfigServiceClient*>(
      io_data_->config_client());
}

DataReductionProxyBypassStats::UnreachableCallback
DataReductionProxyTestContext::unreachable_callback() const {
  return base::Bind(&DataReductionProxySettings::SetUnreachable,
                    base::Unretained(settings_.get()));
}

}  // namespace data_reduction_proxy
