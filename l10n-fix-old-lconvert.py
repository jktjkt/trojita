import sys

ts_context_prefix = "#. ts-context "

context = None
output = []

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
    else:
        output.append(line)

print "\n".join(output)
