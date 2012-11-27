#! /bin/bash

# xgettext-generated .po files use different context than QObject::tr.
# The generated files use something like a file name while QObject::tr expects
# class names. This approach works.

rm -f "${podir}/trojita.ts"
lupdate -silent -recursive src/ -ts "${podir}/trojita.ts"
ts2po --progress=none --pot "${podir}/trojita.ts" "${podir}/trojita_common.pot"
rm "${podir}/trojita.ts"

# ts2po puts class names into source reference comments (#:)
# instead of into extracted comments (#.),
# and does not extract source references at all.
# So convert source reference comments into extracted comments.
sed -i 's/^#:/#./' "${podir}/trojita_common.pot"
