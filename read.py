lst = []
with open('timing.txt') as f:
    for i in f.readlines():
        l = i.strip()
        if l == "" or int(l) > 950:
            lst.append('s')
        # 0 -- 900 ms
        # 1 -- 800 ms
        elif int(l) > 750 and int(l) <  850:
            lst.append(1)
        elif int(l) > 850 and int(l) < 950 :
            lst.append(0)

s = "".join(str(x) for x in lst)

print(s, len(s))