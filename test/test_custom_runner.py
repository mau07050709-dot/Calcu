from platformio.test.result import TestCase, TestStatus
from platformio.test.runners.base import TestRunnerBase


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        if line.startswith("OK:"):
            self.test_suite.add_case(TestCase(line[3:].strip(), TestStatus.PASSED))
        super().on_testing_line_output(line)
