import gzip
import os
import sys
import subprocess

Import("env")

for filename in ['update.html']:
    skip = False
    # coment out next two lines to always rebuild
    if os.path.isfile('assets/minified_' + filename + '.gz'):
        skip = True

    if skip:
        sys.stderr.write(f"prepare_update_html.py: minified_{filename}.gz already available\n")
        continue

    # use html-minifier-terser to reduce size of html/js/css
    # you need to install html-minifier-terser first:
    #   npm install html-minifier-terser -g
    #   see: https://github.com/terser/html-minifier-terser
    cmdline = ['html-minifier-terser', '--remove-comments', '--minify-css', 'true', '--minify-js', '{"compress":{"drop_console":true},"mangle":{"toplevel":true},"nameCache":{}}', '--case-sensitive', '--sort-attributes', '--sort-class-name', '--remove-tag-whitespace', '--collapse-whitespace', '--conservative-collapse', f'assets/{filename}', '-o', f'assets/minified_{filename}']
    subprocess.run(cmdline)  

    # gzip the file
    with open('assets/' + "minified_" + filename, 'rb') as inputFile:
        with gzip.open('assets/minified_' + filename + '.gz', 'wb') as outputFile:
            sys.stderr.write(f"prepare_update_html.py: gzip \'assets/minified_{filename}\' to \'assets/minified_{filename}.gz\'\n")
            outputFile.writelines(inputFile)

    # Delete temporary minified html
    os.remove('assets/' + "minified_" + filename)