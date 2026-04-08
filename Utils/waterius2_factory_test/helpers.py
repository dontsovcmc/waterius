import sys
import time
import re
from typing import Callable
from loguru import logger


class SerialBuffer:
    def __init__(self, api):
        self.lines: list[str] = []
        self.api = api

    def add_lines(self, new_lines: list[str]):
        """Add lines read from serial port"""
        self.lines.extend(new_lines)

    def wait_for_pattern(self, pattern: str, timeout: float = 5.0) -> tuple[int | str | None, str]:
        """
        Waits for a line matching the given pattern with timeout.
        Reads lines from serial port and extracts value from first capturing group if present.
        If pattern has no capturing group, returns True on match.
        Supports both integer and string values.
        Returns that line plus all subsequent lines.
        Removes all lines up to and including the marker from the buffer.

        Args:
            pattern: Regex pattern with or without capturing group
                     With group: r'imp0:(\\d+)' or r'model:([A-Za-z0-9]+)'
                     Without group: r'wifi_ssid' or r'Connected'
            timeout: Timeout in seconds

        Returns:
            Tuple of (extracted_value, matched_line)
            If pattern has capturing group:
                - Extracted value is int if it's all digits, otherwise string.
            If pattern has no capturing group:
                - Returns True on match
            Returns (None, '') if no marker found within timeout
        """
        start_time = time.time()

        while True:
            # Read new lines from serial port
            new_lines = self.api.serial_readlines(wait=5000, delimiter='\n', prefix='00:')
            if new_lines:
                self.add_lines(new_lines)

            # Search for pattern in buffer
            for i, line in enumerate(self.lines):
                match = re.search(pattern, line)
                if match:
                    # Found the marker
                    try:
                        # Try to extract from capturing group if it exists
                        captured_value = match.group(1)
                        # Try to convert to int if it's all digits
                        try:
                            extracted_value = int(captured_value)
                        except ValueError:
                            extracted_value = captured_value
                    except IndexError:
                        # No capturing group, return True
                        extracted_value = True

                    # Remove lines up to marker
                    self.lines = self.lines[i:]
                    return extracted_value, line

            # Check timeout
            if time.time() - start_time > timeout:
                return None, ''

            time.sleep(1.0)


def assert_wait_log(buffer: SerialBuffer,
                    pattern: str,
                    timeout: float,
                    error_comment: str = 'В логе нет',
                    func: Callable[[int | str | bool], bool] | None = None) -> any:
    """
    Waits for a pattern in the buffer and asserts with optional validation function.

    Args:
        buffer: SerialBuffer instance
        pattern: Regex pattern to search
        timeout: Timeout in seconds
        error_comment: Error message prefix
        func: Optional lambda/function to validate the extracted value (e.g., lambda x: x > 0.001)
    """
    out, line = buffer.wait_for_pattern(pattern, timeout)
    if not out:
        for line in buffer.lines:
            logger.info(f'{line}')
        logger.error(f'{error_comment} {pattern}')
        sys.exit(1)

    if func is not None and not func(out):
        logger.error(f'{error_comment} {pattern}')
        sys.exit(1)

    return out
