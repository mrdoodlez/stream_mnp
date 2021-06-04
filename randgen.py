import random
import struct

CMD_RT = 0
CMD_PV = 1

local_timestamp = 0

for f in range(2):
    uvw = []
    for i in range(6):
        bidata = []
        for i in range(1024):
            bidata.append(random.randint(0, 1024))
        bidata.sort()
        uvw.append(bidata)

    fname = "s{0}.dat".format(f)
    fout = open(fname, "wb")

    local_timestamp += random.randint(0, 1024);
    rt_offset = random.randint(0, 8)

    for i in range(1024):
        if i == rt_offset:
            fout.write(struct.pack("<bI", CMD_RT, local_timestamp))
        else:
            fout.write(struct.pack("<b6I", CMD_PV, uvw[0][i], uvw[1][i], uvw[2][i],
                                   uvw[3][i], uvw[4][i], uvw[5][i]))
    fout.close()


    


