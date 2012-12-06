import sys

ts_context_prefix = "#. ts-context "

context = None
output = []
headers_were_sanitized = False

for line in sys.stdin.readlines():
    if line.endswith("\n"):
        line = line[:-1]
    if line.startswith(ts_context_prefix):
        context = line[len(ts_context_prefix):]
    elif line.startswith("msgid "):
        if context is not None:
            output.append('msgctxt "%s|"' % context)
            context = None
        output.append(line)
    elif line == "#, fuzzy":
        pass
    elif not headers_were_sanitized and \
            line == r'"X-Virgin-Header: remove this line if you change anything in the header.\n"':
        # We know that we're on an ancient version of lconvert which produces
        # stuff which require heavy, heavy sanitization.
        # The header in particular is all wrong; it misses the crucial
        # "X-Qt-Contexts" thing without which our sed magic doesn't fix the
        # contexts at all, so better replace it with somethig which actually
        # works.
        # Oh I hate this stuff.
        headers_were_sanitized = True
        output = [
            'msgid ""',
            'msgstr ""',
            r'"MIME-Version: 1.0\n"',
            r'"Content-Type: text/plain; charset=UTF-8\n"',
            r'"Content-Transfer-Encoding: 8bit\n"',
            r'"X-Qt-Contexts: true\n"'
        ]
    else:
        output.append(line)

print "\n".join(output)
