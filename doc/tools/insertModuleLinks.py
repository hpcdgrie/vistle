import sys
import os

# Only handle .md files
if not sys.argv[2].endswith(".md"):
    sys.exit(1)

# Open the input file for reading
input_file_path = sys.argv[2]
try:
    input_file = open(input_file_path, "r")
except IOError as e:
    print(f"Error opening input file: {e}")
    sys.exit(1)

# Create output directory if it does not exist
output_file_path = sys.argv[3]
output_dir = os.path.dirname(output_file_path)
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

# Open the output file for writing
try:
    output_file = open(output_file_path, "w")
except IOError as e:
    print(f"Error opening output file: {e}")
    sys.exit(1)

# Get environment variables
modules = os.environ.get('ALL_VISTLE_MODULES', '').split(" ")
categories = os.environ.get('ALL_VISTLE_MODULES_CATEGORY', '').split(" ")

# Get the relative path from input file to vistle/module directory
this_directory = os.path.dirname(os.path.realpath(__file__))
doc_source_dir = sys.argv[1]
relative_path = os.path.relpath(doc_source_dir, os.path.dirname(output_file_path))

# Search input for occurrences of moduleName.md
for line in input_file:
    for module in modules:
        module_key = "[" + module + "]()"
        if module_key in line:
            category = categories[modules.index(module)]
            link = f"{relative_path}/modules/{category}/{module}.md"
            replacement = f"[{module}]({link})"
            line = line.replace(module_key, replacement)
    output_file.write(line)

# Close the files
input_file.close()
output_file.close()
