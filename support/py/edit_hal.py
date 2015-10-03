#!/usr/bin/env python

from lxml import etree
import getopt
import sys

# opt, optl = 'U:c:x:i:t', ["catalog=", "ini="]
# optlist, args = getopt.getopt(sys.argv[1:], opt, optl)
# optlist = dict(optlist)

if "c" in sys.argv[1:] :
    cls = True
else :
    cls = False


fname = '/usr/share/pyshared/gladevcp/hal_pythonplugin.py'
f = open(fname).read()
if f.find('Features') == -1:
    open(fname, "w").write('from features import Features\n' + f)



fname = '/usr/share/glade3/catalogs/hal_python.xml'
xml = etree.parse(fname)
root = xml.getroot()

dest = root.find('glade-widget-classes')
for n in dest.findall('glade-widget-class'):
    if n.get('name') == 'Features':
        dest.remove(n)
        break

elem = etree.fromstring('''
<glade-widget-class name="Features" generic-name="features" title="features">
    <properties>
        <property id="size" query="False" default="1" visible="False"/>
        <property id="spacing" query="False" default="0" visible="False"/>
        <property id="homogeneous" query="False" default="0" visible="False"/>
    </properties>
</glade-widget-class>''')
if not cls:
    dest.insert(0, elem)

dest = root.find('glade-widget-group')
for n in dest.findall('glade-widget-class-ref'):
    if n.get('name') == 'Features':
        dest.remove(n)
        break

elem = etree.fromstring('<glade-widget-class-ref name="Features"/>')
if not cls:
    dest.insert(0, elem)

xml.write(fname, pretty_print = True)
