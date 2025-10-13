#!/usr/bin/env python3
import subprocess
import xml.etree.ElementTree as ET
import sys

# Path to test binary
test_binary = sys.argv[1]
xml_file = sys.argv[2]

# Run the test binary with XML output
result = subprocess.run([test_binary, f"--gtest_output=xml:{xml_file}"])
exit_code = result.returncode

# Parse XML and print summary
try:
    tree = ET.parse(xml_file)
    root = tree.getroot()
    total = int(root.attrib.get('tests', 0))
    failures = int(root.attrib.get('failures', 0))
    errors = int(root.attrib.get('errors', 0))
    skipped = sum(1 for t in root.findall('.//testcase/skipped'))
    passed = total - failures - errors - skipped
    print("Summary: Total={total}, Passed={passed}, Failures={failures}, Errors={errors}, Skipped={skipped}")
except FileNotFoundError:
    print(f"XML file {xml_file} not found!")

# Exit with the same code as the test binary
sys.exit(exit_code)
