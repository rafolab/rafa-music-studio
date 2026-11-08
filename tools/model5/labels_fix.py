exec(open('/tmp/gw/lab.py').read())
deck=cv2.imread('/tmp/gw/deck.png')   # ChatGPT podklad + moje tlacidla
out=deck.copy()
def comp_mask(im,box,bright_text,band=None,minarea=6):
    x0,y0,x1,y1=box; reg=im[y0:y1,x0:x1]
    g=cv2.cvtColor(reg,cv2.COLOR_BGR2GRAY).astype(int); bg=cv2.cvtColor(cv2.medianBlur(reg,21),cv2.COLOR_BGR2GRAY).astype(int)
    m=(((g-bg) if bright_text else (bg-g))>35).astype(np.uint8)
    n,lab,st,_=cv2.connectedComponentsWithStats(m); keep=np.zeros_like(m)
    for i in range(1,n):
        if st[i,4]<minarea: continue
        if band and not (band[0]<=st[i,1]+y0 and st[i,1]+st[i,3]+y0<=band[1]): continue
        keep[lab==i]=1
    full=np.zeros(im.shape[:2],np.uint8); full[y0:y1,x0:x1]=keep; return full
def row_fill(im,mask):
    im=im.copy().astype(np.float32); ys=np.unique(np.nonzero(mask)[0])
    for y in ys:
        row=mask[y]>0; xs=np.arange(len(row)); good=~row
        for c in range(3): im[y,row,c]=np.interp(xs[row],xs[good],im[y,good,c])
    return im.astype(np.uint8)
def textcolor(im,mask):
    px=im[mask>0]; br=px.mean(1); return np.median(px[br>=np.percentile(br,60)] if px.mean()>100 else px[br<=np.percentile(br,40)],0)

# ---------- tmave napisy na kove ----------
inkcol=ink_color(orig,(1050,765,1150,790))
W_,TR_=650,0.0; SX_=1.03; CAP=13; ANG=3.0
for word,box,band,cx,cy in [
    ('INPUT',(845,740,960,805),(772,805),897,788.5),
    ('VOLUME',(1220,735,1365,790),(752,790),1291,767.5),
    ('PHONES',(1405,640,1540,678),(645,677),1486,668.5)]:
    m_new=comp_mask(out,box,False,band)   # ChatGPT text v aktualnom podklade
    m_new=cv2.dilate(m_new,np.ones((3,3),np.uint8),iterations=2)
    out=row_fill(out,m_new)
    out,_=place(out,word,cx,cy,CAP,ANG,W_,TR_,SX_,inkcol,0.35)

# ---------- svetle napisy na tmavom paneli (od pôvodného podkladu) ----------
def light_label(word,box,band,cx,cy,lines=None):
    global out
    x0,y0,x1,y1=box
    base=orig.copy()
    m_old=comp_mask(base,box,True,band,minarea=4)
    m_old=cv2.dilate(m_old,np.ones((3,3),np.uint8),iterations=2)
    col=textcolor(orig,comp_mask(orig,box,True,band,minarea=4))
    # starý text a ciary prec
    erase=m_old.copy()
    if lines:
        for (lx0,lx1,ly0,ly1) in lines: 
            lm=comp_mask(base,(lx0,ly0,lx1,ly1),True,None,4); erase|=cv2.dilate(lm,np.ones((3,3),np.uint8),iterations=1)
    clean=row_fill(base,erase)
    # vykreslit text
    g=render(word,CAP,600,0,1.0)
    ref=render('STOPY / VRSTVY',CAP,600,0,1.0); sx=150/ref.width
    tmp,(px,py,w,h)=place(clean,word,cx,cy,CAP,ANG,600,0,sx,col,0)
    tw=g.width*sx
    if lines:
        # ciary: povodne pixely tam, kde ostavaju; predlzit kopiou krajneho stlpca
        L,R=cx-tw/2-8, cx+tw/2+10
        for idx,(lx0,lx1,ly0,ly1) in enumerate(lines):
            lm=comp_mask(base,(lx0,ly0,lx1,ly1),True,None,4)
            ys,xs=np.nonzero(lm)
            k=np.polyfit(xs,ys,1)[0]
            if idx==0: rng=(lx0, int(L))
            else: rng=(int(R), lx1)
            for x in range(rng[0],rng[1]+1):
                src=min(max(x,xs.min()),xs.max()); dy=int(round(k*(x-src)))
                colpx=np.nonzero(lm[:,src])[0]
                for yy in range(colpx.min()-1,colpx.max()+2):
                    tmp[yy+dy,x]=base[yy,src]
    out[y0:y1,x0:x1]=tmp[y0:y1,x0:x1]
light_label('TRACKS / LAYERS',(740,540,1132,585),(550,580),924,565.5,lines=[(745,845,564,580),(1006,1128,550,566)])
light_label('METRONOME',(1185,526,1330,560),(528,560),1258,544)
cv2.imwrite('/tmp/gw/deck2.png',out)
def z(b): x0,y0,x1,y1=b; return cv2.resize(out[y0:y1,x0:x1],None,fx=3,fy=3,interpolation=cv2.INTER_LANCZOS4)
rows=[z((840,750,1360,805)),z((740,525,1340,585)),z((1400,640,1540,750))]
W=max(r.shape[1] for r in rows); cv2.imwrite('/tmp/gw/chk.png',np.vstack([cv2.copyMakeBorder(r,4,4,0,W-r.shape[1],cv2.BORDER_CONSTANT,value=(255,255,255)) for r in rows]))
