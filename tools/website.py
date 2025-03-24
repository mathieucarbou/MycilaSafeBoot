import gzip
import os
import sys
import subprocess

Import("env")

os.makedirs(".pio/embed", exist_ok=True)

for filename in ["website.html"]:
    skip = False
    # comment out next two lines to always rebuild
    if os.path.isfile(".pio/embed/" + filename + ".gz"):
        skip = True

    if skip:
        sys.stderr.write(f"website.py: {filename}.gz already available\n")
        continue

    # use html-minifier-terser to reduce size of html/js/css
    # you need to install html-minifier-terser first:
    #   npm install html-minifier-terser -g
    #   see: https://github.com/terser/html-minifier-terser
    subprocess.run(
        [
            "html-minifier-terser",
            "--remove-comments",
            "--minify-css",
            "true",
            "--minify-js",
            '{"compress":{"drop_console":true},"mangle":{"toplevel":true},"nameCache":{}}',
            "--case-sensitive",
            "--sort-attributes",
            "--sort-class-name",
            "--remove-tag-whitespace",
            "--collapse-whitespace",
            "--conservative-collapse",
            f"embed/{filename}",
            "-o",
            f".pio/embed/{filename}",
        ]
    )

    # gzip the file
    with open(".pio/embed/" + filename, "rb") as inputFile:
        with gzip.open(".pio/embed/" + filename + ".gz", "wb") as outputFile:
            sys.stderr.write(
                f"website.py: gzip '.pio/embed/{filename}' to '.pio/embed/{filename}.gz'\n"
            )
            outputFile.writelines(inputFile)

    # Delete temporary minified html
    os.remove(".pio/embed/" + filename)
