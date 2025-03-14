/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <TestImpactFramework/TestImpactClientSequenceReportSerializer.h>
#include <TestImpactFramework/TestImpactSequenceReportException.h>
#include <TestImpactFramework/TestImpactUtils.h>

#include <AzCore/JSON/document.h>
#include <AzCore/JSON/prettywriter.h>
#include <AzCore/JSON/rapidjson.h>
#include <AzCore/JSON/stringbuffer.h>

namespace TestImpact
{
    namespace
    {
        namespace SequenceReportFields
        {
            // Keys for pertinent JSON node and attribute names
            constexpr const char* Keys[] =
            {
                "name",
                "command_args",
                "start_time",
                "end_time",
                "duration",
                "result",
                "num_passing_tests",
                "num_failing_tests",
                "num_disabled_tests",
                "tests",
                "num_passing_test_runs",
                "num_failing_test_runs",
                "num_execution_failure_test_runs",
                "num_timed_out_test_runs",
                "num_unexecuted_test_runs",
                "passing_test_runs",
                "failing_test_runs",
                "execution_failure_test_runs",
                "timed_out_test_runs",
                "unexecuted_test_runs",
                "total_num_passing_tests",
                "total_num_failing_tests",
                "total_num_disabled_tests",
                "total_num_test_runs",
                "num_included_test_runs",
                "num_excluded_test_runs",
                "included_test_runs",
                "excluded_test_runs",
                "execution_failure",
                "coverage_failure",
                "test_failure",
                "integrity_failure",
                "test_sharding",
                "target_output_capture",
                "test_prioritization",
                "dynamic_dependency_map",
                "type",
                "test_target_timeout",
                "global_timeout",
                "max_concurrency",
                "policy",
                "suite",
                "selected_test_runs",
                "selected_test_run_report",
                "total_num_passing_test_runs",
                "total_num_failing_test_runs",
                "total_num_execution_failure_test_runs",
                "total_num_timed_out_test_runs",
                "total_num_unexecuted_test_runs",
                "drafted_test_runs",
                "drafted_test_run_report",
                "discarded_test_runs",
                "discarded_test_run_report"
            };

            enum
            {
                Name,
                CommandArgs,
                StartTime,
                EndTime,
                Duration,
                Result,
                NumPassingTests,
                NumFailingTests,
                NumDisabledTests,
                Tests,
                NumPassingTestRuns,
                NumFailingTestRuns,
                NumExecutionFailureTestRuns,
                NumTimedOutTestRuns,
                NumUnexecutedTestRuns,
                PassingTestRuns,
                FailingTestRuns,
                ExecutionFailureTestRuns,
                TimedOutTestRuns,
                UnexecutedTestRuns,
                TotalNumPassingTests,
                TotalNumFailingTests,
                TotalNumDisabledTests,
                TotalNumTestRuns,
                NumIncludedTestRuns,
                NumExcludedTestRuns,
                IncludedTestRuns,
                ExcludedTestRuns,
                ExecutionFailure,
                CoverageFailure,
                TestFailure,
                IntegrityFailure,
                TestSharding,
                TargetOutputCapture,
                TestPrioritization,
                DynamicDependencyMap,
                Type,
                TestTargetTimeout,
                GlobalTimeout,
                MaxConcurrency,
                Policy,
                Suite,
                SelectedTestRuns,
                SelectedTestRunReport,
                TotalNumPassingTestRuns,
                TotalNumFailingTestRuns,
                TotalNumExecutionFailureTestRuns,
                TotalNumTimedOutTestRuns,
                TotalNumUnexecutedTestRuns,
                DraftedTestRuns,
                DraftedTestRunReport,
                DiscardedTestRuns,
                DiscardedTestRunReport
            };
        } // namespace SequenceReportFields

        AZ::u64 TimePointInMsAsInt64(AZStd::chrono::high_resolution_clock::time_point timePoint)
        {
            return AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(timePoint.time_since_epoch()).count();
        }

        void SerializeTestRunMembers(const Client::TestRunBase& testRun, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            // Name
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Name]);
            writer.String(testRun.GetTargetName().c_str());

            // Command string
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::CommandArgs]);
            writer.String(testRun.GetCommandString().c_str());

            // Start time
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::StartTime]);
            writer.Int64(TimePointInMsAsInt64(testRun.GetStartTime()));

            // End time
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::EndTime]);
            writer.Int64(TimePointInMsAsInt64(testRun.GetEndTime()));

            // Duration
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Duration]);
            writer.Uint64(testRun.GetDuration().count());

            // Result
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Result]);
            writer.String(TestRunResultAsString(testRun.GetResult()).c_str());
        }

        void SerializeTestRun(const Client::TestRunBase& testRun, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            writer.StartObject();
            {
                SerializeTestRunMembers(testRun, writer);
            }
            writer.EndObject();
        }

        void SerializeCompletedTestRun(const Client::CompletedTestRun& testRun, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            writer.StartObject();
            {
                SerializeTestRunMembers(testRun, writer);

                // Number of passing test cases
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumPassingTests]);
                writer.Uint64(testRun.GetTotalNumPassingTests());

                // Number of failing test cases
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumFailingTests]);
                writer.Uint64(testRun.GetTotalNumFailingTests());

                // Number of disabled test cases
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumDisabledTests]);
                writer.Uint64(testRun.GetTotalNumDisabledTests());

                // Tests
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::Tests]);
                writer.StartArray();

                for (const auto& test : testRun.GetTests())
                {
                    // Test
                    writer.StartObject();

                    // Name
                    writer.Key(SequenceReportFields::Keys[SequenceReportFields::Name]);
                    writer.String(test.GetName().c_str());

                    // Result
                    writer.Key(SequenceReportFields::Keys[SequenceReportFields::Result]);
                    writer.String(ClientTestResultAsString(test.GetResult()).c_str());

                    writer.EndObject(); // Test
                }

                writer.EndArray(); // Tests
            }
            writer.EndObject();
        }

        void SerializeTestRunReport(
            const Client::TestRunReport& testRunReport, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            writer.StartObject();
            {
                // Result
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::Result]);
                writer.String(TestSequenceResultAsString(testRunReport.GetResult()).c_str());

                // Start time
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::StartTime]);
                writer.Int64(TimePointInMsAsInt64(testRunReport.GetStartTime()));

                // End time
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::EndTime]);
                writer.Int64(TimePointInMsAsInt64(testRunReport.GetEndTime()));

                // Duration
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::Duration]);
                writer.Uint64(testRunReport.GetDuration().count());

                // Number of passing test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumPassingTestRuns]);
                writer.Uint64(testRunReport.GetNumPassingTestRuns());

                // Number of failing test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumFailingTestRuns]);
                writer.Uint64(testRunReport.GetNumFailingTestRuns());

                // Number of test runs that failed to execute
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumExecutionFailureTestRuns]);
                writer.Uint64(testRunReport.GetNumExecutionFailureTestRuns());

                // Number of timed out test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumTimedOutTestRuns]);
                writer.Uint64(testRunReport.GetNumTimedOutTestRuns());

                // Number of unexecuted test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumUnexecutedTestRuns]);
                writer.Uint64(testRunReport.GetNumUnexecutedTestRuns());

                // Passing test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::PassingTestRuns]);
                writer.StartArray();
                for (const auto& testRun : testRunReport.GetPassingTestRuns())
                {
                    SerializeCompletedTestRun(testRun, writer);
                }
                writer.EndArray(); // Passing test runs

                // Failing test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::FailingTestRuns]);
                writer.StartArray();
                for (const auto& testRun : testRunReport.GetFailingTestRuns())
                {
                    SerializeCompletedTestRun(testRun, writer);
                }
                writer.EndArray(); // Failing test runs

                // Execution failures
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::ExecutionFailureTestRuns]);
                writer.StartArray();
                for (const auto& testRun : testRunReport.GetExecutionFailureTestRuns())
                {
                    SerializeTestRun(testRun, writer);
                }
                writer.EndArray(); // Execution failures

                // Timed out test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::TimedOutTestRuns]);
                writer.StartArray();
                for (const auto& testRun : testRunReport.GetTimedOutTestRuns())
                {
                    SerializeTestRun(testRun, writer);
                }
                writer.EndArray(); // Timed out test runs

                // Unexecuted test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::UnexecutedTestRuns]);
                writer.StartArray();
                for (const auto& testRun : testRunReport.GetUnexecutedTestRuns())
                {
                    SerializeTestRun(testRun, writer);
                }
                writer.EndArray(); // Unexecuted test runs

                // Number of passing tests
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumPassingTests]);
                writer.Uint64(testRunReport.GetTotalNumPassingTests());

                // Number of failing tests
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumFailingTests]);
                writer.Uint64(testRunReport.GetTotalNumFailingTests());

                // Number of disabled tests
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumDisabledTests]);
                writer.Uint64(testRunReport.GetTotalNumDisabledTests());
            }
            writer.EndObject();
        }

        void SerializeTestSelection(
            const Client::TestRunSelection& testSelection, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            writer.StartObject();
            {
                // Total number of test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumTestRuns]);
                writer.Uint64(testSelection.GetTotalNumTests());

                // Number of included test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumIncludedTestRuns]);
                writer.Uint64(testSelection.GetNumIncludedTestRuns());

                // Number of excluded test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::NumExcludedTestRuns]);
                writer.Uint64(testSelection.GetNumExcludedTestRuns());

                // Included test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::IncludedTestRuns]);
                writer.StartArray();
                for (const auto& testRun : testSelection.GetIncludededTestRuns())
                {
                    writer.String(testRun.c_str());
                }
                writer.EndArray(); // Included test runs

                // Excluded test runs
                writer.Key(SequenceReportFields::Keys[SequenceReportFields::ExcludedTestRuns]);
                writer.StartArray();
                for (const auto& testRun : testSelection.GetExcludedTestRuns())
                {
                    writer.String(testRun.c_str());
                }
                writer.EndArray(); // Excluded test runs
            }
            writer.EndObject();
        }

        void SerializePolicyStateBaseMembers(const PolicyStateBase& policyState, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            // Execution failure
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::ExecutionFailure]);
            writer.String(ExecutionFailurePolicyAsString(policyState.m_executionFailurePolicy).c_str());
            
            // Failed test coverage
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::CoverageFailure]);
            writer.String(FailedTestCoveragePolicyAsString(policyState.m_failedTestCoveragePolicy).c_str());
            
            // Test failure
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TestFailure]);
            writer.String(TestFailurePolicyAsString(policyState.m_testFailurePolicy).c_str());
            
            // Integrity failure
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::IntegrityFailure]);
            writer.String(IntegrityFailurePolicyAsString(policyState.m_integrityFailurePolicy).c_str());
            
            // Test sharding
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TestSharding]);
            writer.String(TestShardingPolicyAsString(policyState.m_testShardingPolicy).c_str());
            
            // Target output capture
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TargetOutputCapture]);
            writer.String(TargetOutputCapturePolicyAsString(policyState.m_targetOutputCapture).c_str());
        }

        void SerializePolicyStateMembers(
            const SequencePolicyState& policyState, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            SerializePolicyStateBaseMembers(policyState.m_basePolicies, writer);
        }

        void SerializePolicyStateMembers(
            const SafeImpactAnalysisSequencePolicyState& policyState, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            SerializePolicyStateBaseMembers(policyState.m_basePolicies, writer);

            // Test prioritization
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TestPrioritization]);
            writer.String(TestPrioritizationPolicyAsString(policyState.m_testPrioritizationPolicy).c_str());
        }

        void SerializePolicyStateMembers(
            const ImpactAnalysisSequencePolicyState& policyState, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            SerializePolicyStateBaseMembers(policyState.m_basePolicies, writer);

            // Test prioritization
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TestPrioritization]);
            writer.String(TestPrioritizationPolicyAsString(policyState.m_testPrioritizationPolicy).c_str());

            // Dynamic dependency map
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::DynamicDependencyMap]);
            writer.String(DynamicDependencyMapPolicyAsString(policyState.m_dynamicDependencyMap).c_str());
        }

        template<typename PolicyStateType>
        void SerializeSequenceReportBaseMembers(
            const Client::SequenceReportBase<PolicyStateType>& sequenceReport, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            // Type
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Type]);
            writer.String(SequenceReportTypeAsString(sequenceReport.GetType()).c_str());

            // Test target timeout
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TestTargetTimeout]);
            writer.Uint64(sequenceReport.GetTestTargetTimeout().value_or(AZStd::chrono::milliseconds{0}).count());

            // Global timeout
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::GlobalTimeout]);
            writer.Uint64(sequenceReport.GetGlobalTimeout().value_or(AZStd::chrono::milliseconds{ 0 }).count());

            // Maximum concurrency
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::MaxConcurrency]);
            writer.Uint64(sequenceReport.GetMaxConcurrency());

            // Policies
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Policy]);
            writer.StartObject();
            {
                SerializePolicyStateMembers(sequenceReport.GetPolicyState(), writer);
            }
            writer.EndObject(); // Policies

            // Suite
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Suite]);
            writer.String(SuiteTypeAsString(sequenceReport.GetSuite()).c_str());

            // Selected test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::SelectedTestRuns]);
            SerializeTestSelection(sequenceReport.GetSelectedTestRuns(), writer);

            // Selected test run report
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::SelectedTestRunReport]);
            SerializeTestRunReport(sequenceReport.GetSelectedTestRunReport(), writer);

            // Start time
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::StartTime]);
            writer.Int64(TimePointInMsAsInt64(sequenceReport.GetStartTime()));

            // End time
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::EndTime]);
            writer.Int64(TimePointInMsAsInt64(sequenceReport.GetEndTime()));

            // Duration
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Duration]);
            writer.Uint64(sequenceReport.GetDuration().count());

            // Result
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::Result]);
            writer.String(TestSequenceResultAsString(sequenceReport.GetResult()).c_str());

            // Total number of test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumTestRuns]);
            writer.Uint64(sequenceReport.GetTotalNumTestRuns());

            // Total number of passing test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumPassingTestRuns]);
            writer.Uint64(sequenceReport.GetTotalNumPassingTestRuns());

            // Total number of failing test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumFailingTestRuns]);
            writer.Uint64(sequenceReport.GetTotalNumFailingTestRuns());

            // Total number of test runs that failed to execute
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumExecutionFailureTestRuns]);
            writer.Uint64(sequenceReport.GetTotalNumExecutionFailureTestRuns());

            // Total number of timed out test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumTimedOutTestRuns]);
            writer.Uint64(sequenceReport.GetTotalNumTimedOutTestRuns());

            // Total number of unexecuted test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumUnexecutedTestRuns]);
            writer.Uint64(sequenceReport.GetTotalNumUnexecutedTestRuns());

            // Total number of passing tests
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumPassingTests]);
            writer.Uint64(sequenceReport.GetTotalNumPassingTests());

            // Total number of failing tests
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumFailingTests]);
            writer.Uint64(sequenceReport.GetTotalNumFailingTests());

             // Total number of disabled tests
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::TotalNumDisabledTests]);
            writer.Uint64(sequenceReport.GetTotalNumDisabledTests());
        }

        template<typename PolicyStateType>
        void SerializeDraftingSequenceReportMembers(
            const Client::DraftingSequenceReportBase<PolicyStateType>& sequenceReport, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
        {
            SerializeSequenceReportBaseMembers(sequenceReport, writer);

            // Drafted test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::DraftedTestRuns]);
            writer.StartArray();
            for (const auto& testRun : sequenceReport.GetDraftedTestRuns())
            {
                writer.String(testRun.c_str());
            }
            writer.EndArray(); // Drafted test runs

            // Drafted test run report
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::DraftedTestRunReport]);
            SerializeTestRunReport(sequenceReport.GetDraftedTestRunReport(), writer);
        }
    } // namespace

    AZStd::string SerializeSequenceReport(const Client::RegularSequenceReport& sequenceReport)
    {
        rapidjson::StringBuffer stringBuffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(stringBuffer);

        writer.StartObject();
        {
            SerializeSequenceReportBaseMembers(sequenceReport, writer);
        }
        writer.EndObject();

        return stringBuffer.GetString();
    }

    AZStd::string SerializeSequenceReport(const Client::SeedSequenceReport& sequenceReport)
    {
        rapidjson::StringBuffer stringBuffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(stringBuffer);

        writer.StartObject();
        {
            SerializeSequenceReportBaseMembers(sequenceReport, writer);
        }
        writer.EndObject();

        return stringBuffer.GetString();
    }

    AZStd::string SerializeSequenceReport(const Client::ImpactAnalysisSequenceReport& sequenceReport)
    {
        rapidjson::StringBuffer stringBuffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(stringBuffer);

        writer.StartObject();
        {
            SerializeDraftingSequenceReportMembers(sequenceReport, writer);

            // Discarded test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::DiscardedTestRuns]);
            writer.StartArray();
            for (const auto& testRun : sequenceReport.GetDiscardedTestRuns())
            {
                writer.String(testRun.c_str());
            }
            writer.EndArray(); // Discarded test runs
        }
        writer.EndObject();

        return stringBuffer.GetString();
    }

    AZStd::string SerializeSequenceReport(const Client::SafeImpactAnalysisSequenceReport& sequenceReport)
    {
        rapidjson::StringBuffer stringBuffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(stringBuffer);

        writer.StartObject();
        {
            SerializeDraftingSequenceReportMembers(sequenceReport, writer);

            // Discarded test runs
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::DiscardedTestRuns]);
            SerializeTestSelection(sequenceReport.GetDiscardedTestRuns(), writer);

            // Discarded test run report
            writer.Key(SequenceReportFields::Keys[SequenceReportFields::DiscardedTestRunReport]);
            SerializeTestRunReport(sequenceReport.GetDiscardedTestRunReport(), writer);
        }
        writer.EndObject();

        return stringBuffer.GetString();
    }
} // namespace TestImpact
