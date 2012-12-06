import os
import re
import subprocess

"""Fetch the .po files from KDE's SVN for Trojita

Run me from Trojita's top-level directory.
"""


SVN_PATH = "svn://anonsvn.kde.org/home/kde/trunk/l10n-kde4/"
SOURCE_PO_PATH = "/messages/kdereview/trojita_common.po"
OUTPUT_PO_PATH = "./po/"
OUTPUT_PO_PATTERN = "trojita_common_%s.po"

fixer = re.compile(r'^#~\| ', re.MULTILINE)

if not os.path.exists(OUTPUT_PO_PATH):
    os.mkdir(OUTPUT_PO_PATH)

all_languages = subprocess.check_output(['svn', 'cat', SVN_PATH + 'subdirs'],
                                       stderr=subprocess.STDOUT)

all_languages = [x.strip() for x in all_languages.split("\n") if len(x)]
for lang in all_languages:
    try:
        raw_data = subprocess.check_output(['svn', 'cat', SVN_PATH + lang + SOURCE_PO_PATH],
                                          stderr=subprocess.PIPE)
        (transformed, subs) = fixer.subn('# ~| ', raw_data)
        if (subs > 0):
            print "Fetched %s (and performed %d cleanups)" % (lang, subs)
        else:
            print "Fetched %s" % lang
        file(OUTPUT_PO_PATH + OUTPUT_PO_PATTERN % lang, "wb").write(transformed)
    except subprocess.CalledProcessError:
        print "No data for %s" % lang
