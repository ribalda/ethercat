#!/usr/bin/python

#-----------------------------------------------------------------------------
#
# $Id$
#
# Convert a slave information file to a slave description Xml.
#
#-----------------------------------------------------------------------------

from xml.dom.minidom import Document
import sys
import re
import os
import getopt

#-----------------------------------------------------------------------------

infoFileName = 'info'

#-----------------------------------------------------------------------------

class PdoEntry:
    def __init__(self, index, subindex, name, bitlength):
        self.index = index
        self.subindex = subindex
        self.name = name
        self.bitlength = bitlength

    def toXml(self, doc, element):
        entryElement = doc.createElement('Entry')

        indexElement = doc.createElement('Index')
        indexText = doc.createTextNode('#x%04x' % self.index)
        indexElement.appendChild(indexText)
        entryElement.appendChild(indexElement)

        if (self.index != 0):
            subIndexElement = doc.createElement('SubIndex')
            subIndexText = doc.createTextNode(str(self.subindex))
            subIndexElement.appendChild(subIndexText)
            entryElement.appendChild(subIndexElement)

        lengthElement = doc.createElement('BitLen')
        lengthText = doc.createTextNode(str(self.bitlength))
        lengthElement.appendChild(lengthText)
        entryElement.appendChild(lengthElement)

        if (self.index != 0):
            nameElement = doc.createElement('Name')
            nameText = doc.createTextNode(self.name)
            nameElement.appendChild(nameText)
            entryElement.appendChild(nameElement)

            dataTypeElement = doc.createElement('DataType')
            dataTypeText = doc.createTextNode(self.dataType())
            dataTypeElement.appendChild(dataTypeText)
            entryElement.appendChild(dataTypeElement)

        element.appendChild(entryElement)

    def dataType(self):
        if self.bitlength == 1:
            return 'BOOL'
        elif self.bitlength % 8 == 0:
            if self.bitlength <= 64:
                return 'UINT%u' % self.bitlength
            else:
                return 'STRING(%u)' % (self.bitlength / 8)
        else:
            assert False, 'Invalid bit length %u' % self.bitlength 

#-----------------------------------------------------------------------------

class Pdo:
    def __init__(self, dir, index):
        self.dir = dir
        self.index = index
        self.entries = []

    def appendEntry(self, entry):
        self.entries.append(entry)

    def toXml(self, doc, element):
        pdoElement = doc.createElement('%sxPdo' % self.dir)

        indexElement = doc.createElement('Index')
        indexText = doc.createTextNode('#x%04x' % self.index)
        indexElement.appendChild(indexText)
        pdoElement.appendChild(indexElement)

        nameElement = doc.createElement('Name')
        pdoElement.appendChild(nameElement)

        for e in self.entries:
            e.toXml(doc, pdoElement)

        element.appendChild(pdoElement)

#-----------------------------------------------------------------------------

class Device:
    def __init__(self):
        self.vendor = 0
        self.product = 0
        self.revision = 0
        self.pdos = []

    def parseInfoFile(self, fileName):
        reVendor = re.compile('Vendor ID:.*\((\d+)\)')
        reProduct = re.compile('Product code:.*\((\d+)\)')
        reRevision = re.compile('Revision number:.*\((\d+)\)')
        rePdo = re.compile('([RT])xPdo\s+0x([0-9A-F]+)')
        rePdoEntry = \
            re.compile('0x([0-9A-F]+):([0-9A-F]+),\s+(\d+) bit,\s+"([^"]*)"')
        pdo = None
        f = open(fileName, 'r')
        while True:
            line = f.readline()
            if not line: break

            match = reVendor.search(line)
            if match:
                self.vendor = int(match.group(1))

            match = reProduct.search(line)
            if match:
                self.product = int(match.group(1))

            match = reRevision.search(line)
            if match:
                self.revision = int(match.group(1))

            match = rePdo.search(line)
            if match:
                pdo = Pdo(match.group(1), int(match.group(2), 16))
                self.pdos.append(pdo)

            match = rePdoEntry.search(line)
            if match:
                pdoEntry = PdoEntry(int(match.group(1), 16), \
                    int(match.group(2), 16), match.group(4), \
                    int(match.group(3)))
                pdo.appendEntry(pdoEntry)

        f.close()

    def toXmlDocument(self):
        doc = Document()

        rootElement = doc.createElement('EtherCATInfo')
        doc.appendChild(rootElement)

        vendorElement = doc.createElement('Vendor')
        rootElement.appendChild(vendorElement)

        vendorIdElement = doc.createElement('Id')

        idText = doc.createTextNode(str(self.vendor))
        vendorIdElement.appendChild(idText)

        vendorElement.appendChild(vendorIdElement)

        descriptionsElement = doc.createElement('Descriptions')
        rootElement.appendChild(descriptionsElement)

        devicesElement = doc.createElement('Devices')
        descriptionsElement.appendChild(devicesElement)

        deviceElement = doc.createElement('Device')
        devicesElement.appendChild(deviceElement)

        typeElement = doc.createElement('Type')
        typeElement.setAttribute('ProductCode', '#x%08x' % self.product)
        typeElement.setAttribute('RevisionNo', '#x%08x' % self.revision)
        deviceElement.appendChild(typeElement)

        for p in self.pdos:
            p.toXml(doc, deviceElement)

        return doc

#-----------------------------------------------------------------------------

def usage():
    print """slaveinfo2xml.py [OPTIONS] [FILE]
    File defaults to 'info'.
    Options:
        -h Print this help."""

#-----------------------------------------------------------------------------

try:
    opts, args = getopt.getopt(sys.argv[1:], "h", ["help"])
except getopt.GetoptError, err:
    print str(err)
    usage()
    sys.exit(2)

if len(args) > 1:
    print "Only one argument allowed!"
    usage()
    sys.exit(2)
elif len(args) == 1:
    infoFileName = args[0]

for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    else:
        assert False, "unhandled option"

d = Device()
d.parseInfoFile(infoFileName)
doc = d.toXmlDocument()

# Print our newly created XML
print doc.toprettyxml(indent='  ')

#-----------------------------------------------------------------------------

