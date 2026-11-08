exec(open('/tmp/gw/lab.py').read())
orig=cv2.imread('/mnt/user-data/uploads/c0d80071-image.png')
src=open('/tmp/gw/labels_fix.py').read().split("# ---------- tmave")[0].split("deck=cv2.imread")[0]
exec(open('/tmp/gw/labels_fix.py').read().split("deck=cv2.imread('/tmp/gw/deck.png')")[0].replace("exec(open('/tmp/gw/lab.py').read())",""))
# helper defs from labels_fix (comp_mask,row_fill,textcolor)
body=open('/tmp/gw/labels_fix.py').read()
exec(body[body.index("def comp_mask"):body.index("# ---------- tmave")])
out=orig.copy()
inkcol=ink_color(orig,(1050,765,1150,790))
CAP=13; ANG=3.0
for word,box,band,cx,cy in [
    ('INPUT',(845,740,960,805),(772,805),897,788.5),
    ('MONITOR',(1040,745,1165,795),(765,795),1099.5,777.5),
    ('VOLUME',(1220,735,1365,790),(752,790),1291,767.5),
    ('PHONES',(1405,640,1540,678),(645,678),1486,668.5)]:
    m=comp_mask(orig,box,False,band)
    m=cv2.dilate(m,np.ones((3,3),np.uint8),iterations=2)
    out=row_fill(out,m)
    out,_=place(out,word,cx,cy,CAP,ANG,650,0,1.03,inkcol,0.35)
# svetle napisy na tmavom paneli
exec(body[body.index("def light_label"):body.index("light_label('TRACKS")].replace("base=orig.copy()","base=out.copy()").replace("out[y0:y1,x0:x1]=tmp[y0:y1,x0:x1]","xe=x1+(14 if word=='METRONOME' else 0); ye=min(y1,553) if word=='METRONOME' else y1; out[y0:ye,x0:xe]=tmp[y0:ye,x0:xe]"))
light_label('TRACKS / LAYERS',(740,540,1132,585),(550,580),937,564.8,lines=[(745,845,564,580),(1006,1128,550,566)])
light_label('METRONOME',(1185,526,1316,560),(528,560),1260,542.5)
# dokreslit koniec slova, ktory presiahol hranicu boxu (bez zasahu do napisu ON)
_tmp,_=place(out.copy(),'X',0,0,1,0,600,0,1,(0,0,0),0) if False else (None,None)
# tlacidla vpravo hore
hsv=cv2.cvtColor(orig,cv2.COLOR_BGR2HSV)
for word,(y0,y1,x0,x1),dx in [('PROJECT',(151,170,1359,1426),0),('TRACKS',(199,217,1374,1426),0),('EFFECTS',(246,265,1381,1440),4),('SETTINGS',(295,315,1368,1461),0)]:
    sub=orig[y0:y1,x0:x1].astype(int); sh=hsv[y0:y1,x0:x1]
    m=((sub.mean(2)>95)&(sh[...,1]<90)).astype(np.uint8); ys,xs=np.nonzero(m)
    txt=sub[m>0]; col=txt[np.argsort(txt.mean(1))[-max(5,len(txt)//4):]].mean(0)
    capH=12; ctr=(x0+xs.mean()+dx, y0+(ys.min()+ys.max())/2)
    if word=='EFFECTS': ctr=(1415.5,ctr[1])
    mk=np.zeros(orig.shape[:2],np.uint8); mk[y0:y1,x0:x1]=cv2.dilate(m,np.ones((3,3),np.uint8),iterations=2)
    out=cv2.inpaint(out,mk,4,cv2.INPAINT_TELEA)
    # sirka: rovnaky pomer ako ostatne napisy (Montserrat SemiBold), max sirka tlacidla
    g=render(word,capH,600,0,1.0); sx=min(1.0, (62 if word=='EFFECTS' else 76)/g.width)
    out,_=place(out,word,ctr[0],ctr[1],capH,2.0,600,0,sx,tuple(col),0)
    print(word,capH,g.width,sx)
cv2.imwrite('/tmp/gw/plate_labels.png',out)
def z(b,f=3): x0,y0,x1,y1=b; return cv2.resize(out[y0:y1,x0:x1],None,fx=f,fy=f,interpolation=cv2.INTER_LANCZOS4)
rows=[z((840,750,1360,805)),z((740,525,1340,585)),z((1330,135,1540,330),2)]
W=max(r.shape[1] for r in rows); cv2.imwrite('/tmp/gw/chk3.png',np.vstack([cv2.copyMakeBorder(r,4,4,0,W-r.shape[1],cv2.BORDER_CONSTANT,value=(255,255,255)) for r in rows]))
