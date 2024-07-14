import sys

if len(sys.argv) != 2:
  print("%s <bin-file>")
  sys.exit(-1)

f = open(sys.argv[1], "rb")
print("typedef unsigned char byte;")
print("byte rom_image[0x8000] = {")

rows = []
row = []
rc = 0
for b in f.read():
  row.append("0x%02x" % b)
  rc += 1
  if rc == 8:
    rows.append(', '.join(row))
    row = []
    rc = 0

for r in rows[:-2]:
  print('  ' + r + ',')
print('  ' + rows[-1])

print('};')

f.close()
