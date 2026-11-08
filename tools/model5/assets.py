import cv2, numpy as np, json
from PIL import Image
plate=cv2.imread('/tmp/gw/plate_step1.png').astype(np.float32)
src=plate.copy()  # povodny stav pred upravami (pre vyrezy)
H,W=plate.shape[:2]
ctl={'stage':{'width':W,'height':H},'knobs':{},'faders':{},'buttons':{},'leds':{},'vu':{},'display':{}}
def save_sprite(name,bgr,alpha):
    rgba=np.dstack([bgr[...,::-1],alpha*255]).clip(0,255).astype(np.uint8)
    Image.fromarray(rgba,'RGBA').save(f'/tmp/ui2/assets/{name}.webp',quality=92,method=6)
def lum(a): return a[...,0]*0.114+a[...,1]*0.587+a[...,2]*0.299

# ---------- 1. gombiky: odstranit povodne rysky, zapisat elipsy ----------
kn=json.load(open('/tmp/gw/knobs.json'))
kn['jog']=dict(cx=1268,cy=226,w=110,h=100)
kn['input']=dict(cx=897.5,cy=708,w=59,h=50)
kn['monitor']=dict(cx=1097.5,cy=697,w=59,h=55)
kn['volume']=dict(cx=1289.5,cy=687.5,w=61,h=55)
mask=np.zeros((H,W),np.uint8)
for n in ('ch1','ch2','ch3','ch4','metro'):
    k=kn[n]; cx,cy=k['cx'],k['cy']; rx=min(k['w'],k['h'])/2; ry=rx
    x0,x1=int(cx-6),int(cx+7); y0,y1=int(cy-max(k['w'],k['h'])/2-2),int(cy-3)
    reg=lum(plate[y0:y1,x0:x1]); m=(reg<110).astype(np.uint8)
    mask[y0:y1,x0:x1]|=cv2.dilate(m,np.ones((3,3),np.uint8),iterations=1)
plate=cv2.inpaint(plate.astype(np.uint8),mask*255,4,cv2.INPAINT_TELEA).astype(np.float32)
# elipsa: pre kreslenie rysky (os X a Y), uhol zobrazenia ignorujeme (male naklonenie)
for n,k in kn.items():
    rx=max(k['w'],k['h'])/2 if n in('ch1','ch2','ch3','ch4','metro','tempo') else k['w']/2
    ry=min(k['w'],k['h'])/2 if n in('ch1','ch2','ch3','ch4','metro','tempo') else k['h']/2
    ctl['knobs'][n]={'cx':round(k['cx'],1),'cy':round(k['cy'],1),'rx':round(rx,1),'ry':round(ry,1)}

# ---------- 2. fadery ----------
F={'f1':dict(top=(255,522),bot=(280,720),marks={6:538,0:597,-12:655,-100:710},cap=(240,550,294,610),groove=574),
   'f2':dict(top=(381.7,517),bot=(410,712),marks={6:532,0:591,-12:651,-100:704},cap=(373,610,432,672),groove=635),
   'f3':dict(top=(505,510),bot=(537,707),marks={6:526,0:586,-12:645,-100:699},cap=(493,547,550,607),groove=570),
   'f4':dict(top=(628,505),bot=(665,698),marks={6:520,0:580,-12:640,-100:692},cap=(630,613,688,673),groove=637)}
orig_plate=plate.copy()
for n,f in F.items():
    (tx,ty),(bx,by)=f['top'],f['bot']; slant=(bx-tx)/(by-ty)
    slotx=lambda y: tx+(y-ty)*slant
    x0,y0,x1,y1=f['cap']
    Wy0,Wy1=int(max(y0-26,ty+14)),int(min(y1+36,by-14))
    if f['groove']<600: cs,ce=Wy1+2,int(by-14)
    else: cs,ce=int(ty+14),Wy0-2
    # perioda ryzok z autokorelacie
    ysr=np.arange(cs,ce); col=np.array([lum(orig_plate[y,int(slotx(y))+18]) for y in ysr]); col-=col.mean()
    ac=[(col[:-k]*col[k:]).mean() for k in range(10,22)]; per=10+int(np.argmax(ac))
    Lp=per*max(1,(ce-cs)//per)
    rows=[]
    for yy in range(Wy0,Wy1):
        sx=slotx(yy); xl=int(round(sx-46)); xr=int(round(sx+40))
        sy=cs+((yy-cs)%Lp); off=int(round(slotx(sy)-sx))
        rows.append((yy,xl,xr,off,sy))
    for (yy,xl,xr,off,sy) in rows:
        seg=orig_plate[sy,xl+off:xr+off].copy()
        lab_end=int(round(slotx(yy)-29))-xl
        panel=np.median(orig_plate[sy,xl+off:xl+off+lab_end].reshape(-1,3),0)
        seg[:lab_end]=panel
        plate[yy,xl:xr]=seg
    # vratit povodne napisy (0, +6...) v stlpci napisov, zosvetlene o tien
    for (yy,xl,xr,off,sy) in rows:
        lab_end=int(round(slotx(yy)-29))
        o=orig_plate[yy,xl:lab_end]; L=lum(o)
        m=L>70
        if m.any():
            plate[yy,xl:lab_end][m]=np.clip(o[m]*1.25,0,255)
    # mäkké okraje okna
    Wx0=int(min(r[1] for r in rows)); Wx1=int(max(r[2] for r in rows))
    blk=plate[Wy0-8:Wy1+8,Wx0-8:Wx1+8]; ob=orig_plate[Wy0-8:Wy1+8,Wx0-8:Wx1+8]
    msk=np.zeros(blk.shape[:2],np.float32)
    for (yy,xl,xr,off,sy) in rows: msk[yy-Wy0+8, xl-Wx0+8+3: xr-Wx0+8-3]=1
    msk[:12]*=0; msk[-12:]*=0
    msk=cv2.GaussianBlur(msk,(0,0),3.5)[...,None]
    plate[Wy0-8:Wy1+8,Wx0-8:Wx1+8]=ob*(1-msk)+blk*msk
    # sprite hlavicky z povodneho
    S=(x0-8,y0-10,x1+12,y1+20)
    o=orig_plate[S[1]:S[3],S[0]:S[2]]; r=plate[S[1]:S[3],S[0]:S[2]]
    d=np.abs(o-r).max(2); al=np.clip((d-8)/26,0,1); al=cv2.GaussianBlur(al,(3,3),0)
    inner=np.zeros_like(al); inner[(y0-S[1])+2:(y1-S[1])-1,(x0-S[0])+2:(x1-S[0])-2]=1
    al=np.maximum(al,inner)
    save_sprite(f'fader_{n}',o,al)
    ctl['faders'][n]={'sprite':f'assets/fader_{n}.webp','x':S[0],'y':S[1],'w':S[2]-S[0],'h':S[3]-S[1],
        'grooveY':f['groove'],'slant':round(slant,4),'marks':{str(k):v for k,v in f['marks'].items()},
        'track':int(n[1])-1}
cv2.imwrite('/tmp/gw/plate_nofaders.png',plate.astype(np.uint8))
json.dump(ctl,open('/tmp/gw/ctl_partial.json','w'),indent=1)

# ---------- 3. tlacidla ----------
B={ # id: (x,y,w,h) z detekcie, typ
 'project':(1330,148,113,41,'menu'),'tracks':(1336,196,115,41,'menu'),'effects':(1342,245,117,42,'menu'),'settings':(1348,292,119,38,'menu'),
 'rewind':(730,446,102,66,'grey'),'stop':(840,441,101,65,'grey'),'play':(950,436,103,65,'grey'),'rec':(1061,430,117,72,'rec'),
 'loopIn':(1231,426,86,59,'grey'),'loopOut':(1326,421,88,59,'grey'),'loop':(1417,416,91,68,'litgrey'),
 'layer1':(753,594,80,61,'litgrey'),'layer2':(837,591,74,51,'grey'),'layer3':(915,588,73,50,'grey'),'layer4':(992,584,73,50,'grey'),'layerAdd':(1074,580,73,50,'grey'),
 'metroOn':(1308,573,69,45,'litgrey'),
 'mute1':(240,743,41,37,'cream'),'solo1':(290,742,40,36,'grey2'),'mute2':(367,734,48,46,'litcream'),'solo2':(417,735,43,36,'grey2'),
 'mute3':(501,731,40,40,'cream'),'solo3':(549,728,40,36,'grey2'),'mute4':(628,724,41,36,'cream'),'solo4':(676,722,40,35,'grey2'),
}
P=plate.copy()
def crop(r,pad=(4,4,5,10)):
    x,y,w,h=r[:4]; return (x-pad[0],y-pad[1],x+w+pad[2],y+h+pad[3])
def pix(name):
    x0,y0,x1,y1=crop(B[name],(0,0,0,0)); return P[y0:y1,x0:x1].reshape(-1,3)
def remap(src,refpix):
    L=lum(src); Lr=lum(refpix)
    order=np.argsort(Lr); refs=refpix[order]; Lrs=Lr[order]
    # kvantilove mapovanie jasu zdroja na jas referencie
    q=(np.argsort(np.argsort(L.ravel()))/(L.size-1))
    idx=np.clip((q*(len(refs)-1)).astype(int),0,len(refs)-1)
    # vyhladit farby referencie podla jasu
    k=max(3,len(refs)//60); sm=np.array([refs[max(0,i-k):i+k].mean(0) for i in range(len(refs))])
    return sm[idx].reshape(src.shape)
def glyph_alpha(src):
    L=lum(src); bl=cv2.GaussianBlur(L,(0,0),5)
    ga=np.clip((bl-L-14)/30,0,1)
    inner=np.zeros_like(ga); h,w=ga.shape; inner[int(h*0.18):int(h*0.78),int(w*0.18):int(w*0.85)]=1
    return cv2.GaussianBlur(ga*inner,(3,3),0)
def erase(src,ga):
    m=(cv2.dilate((ga>0.15).astype(np.uint8),np.ones((3,3),np.uint8),iterations=2)*255)
    return cv2.inpaint(np.clip(src,0,255).astype(np.uint8),m,4,cv2.INPAINT_TELEA).astype(np.float32)
def region(name):
    x0,y0,x1,y1=crop(B[name]); return P[y0:y1,x0:x1].copy()
def clean(name):
    s_=region(name); return erase(s_,glyph_alpha(s_))
def core(img):  # len vnutro tlacidla pre paletu
    h,w=img.shape[:2]; return img[6:h-12,6:w-6].reshape(-1,3)
pal={'orange':core(clean('layer1')),'grey':core(clean('layer2')),'greyOut':core(clean('loopOut')),'cream':core(clean('mute1')),'creamlit':core(clean('mute2'))}
def ink(name):
    s_=region(name); ga=glyph_alpha(s_); return s_[ga>0.7].mean(0)
ink_off=ink('layer2'); ink_on=ink('layer1'); ink_m_off=ink('mute1'); ink_m_on=ink('mute2')
def compose(src,palette,inkcol,strength=1.0,flatten=False,smooth=False):
    ga=glyph_alpha(src); base=erase(src,ga)
    if flatten:
        h,w=base.shape[:2]; core_=np.zeros((h,w),np.float32); core_[6:h-12,6:w-6]=1
        Lb=lum(base); low=cv2.GaussianBlur(Lb,(0,0),9); edge=cv2.GaussianBlur(core_,(0,0),3)
        target=np.median(Lb[core_>0])
        base=base*(1+edge*(target/np.maximum(low,1)-1))[...,None]
    if smooth: base=cv2.bilateralFilter(np.clip(base,0,255).astype(np.uint8),7,40,7).astype(np.float32)
    col=remap(base,palette)
    return col*(1-ga[...,None]*strength)+inkcol*ga[...,None]*strength
off_imgs={}; on_imgs={}
for n,r in B.items():
    x0,y0,x1,y1=crop(r); src=P[y0:y1,x0:x1].copy(); kind=r[4]
    bx,by,bw,bh=r[:4]
    m=np.zeros(src.shape[:2],np.float32); m[4-1:4+bh+7,4-1:4+bw+2]=1; m=cv2.GaussianBlur(m,(3,3),0)
    def from_ref(refname,inkcol):
        rb=B[refname]; rx0,ry0,rx1,ry1=crop(rb); refimg=clean(refname)
        # mapovanie tela referencie na telo cieľa (posun + mierka)
        sx_=bw/rb[2]; sy_=bh/rb[3]
        M=np.float32([[sx_,0,4-4*sx_],[0,sy_,4-4*sy_]])
        warped=cv2.warpAffine(refimg,M,(src.shape[1],src.shape[0]),flags=cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE)
        ga=glyph_alpha(src)
        return warped*(1-ga[...,None])+inkcol*ga[...,None]
    REF={'layer1':'layer2','loop':'loopOut','metroOn':'layer4','mute2':'mute1'}
    if kind=='grey': off=src; on=compose(src,pal['orange'],ink_on,smooth=True)
    elif kind=='litgrey': on=src; off=from_ref(REF[n],ink_off)
    elif kind=='cream': off=src; on=compose(src,pal['creamlit'],ink_m_on)
    elif kind=='litcream': on=src; off=from_ref(REF[n],ink_m_off)
    elif kind=='grey2': off=src; on=compose(src,pal['creamlit'],ink_m_on)
    elif kind=='rec':
        on=src; hsv=cv2.cvtColor(np.clip(src,0,255).astype(np.uint8),cv2.COLOR_BGR2HSV).astype(np.float32)
        hsv[...,2]*=0.52; hsv[...,1]*=0.85; off=cv2.cvtColor(hsv.clip(0,255).astype(np.uint8),cv2.COLOR_HSV2BGR).astype(np.float32)
    elif kind=='menu':
        off=src; L=lum(src); t=np.clip((L-90)/80,0,1)[...,None]
        on=src*(1-t)+np.array([90,190,255],np.float32)*t
    off=np.where(m[...,None]>0,off,src); on=np.where(m[...,None]>0,on,src)
    plate[y0:y1,x0:x1]=src*(1-m[...,None])+off*m[...,None]
    save_sprite(f'btn_{n}_off',off,m); save_sprite(f'btn_{n}_on',on,m)
    ctl['buttons'][n]={'x':x0,'y':y0,'w':x1-x0,'h':y1-y0,'off':f'assets/btn_{n}_off.webp','on':f'assets/btn_{n}_on.webp','kind':kind}

# svatozar okolo povodne svietiacich tlacidiel: ubrat oranzovy presah
for n in ('layer1','mute2','loop','metroOn'):
    x0,y0,x1,y1=crop(B[n]); R=34
    X0,Y0,X1,Y1=x0-R,y0-R,x1+R,y1+R
    blk=plate[Y0:Y1,X0:X1]; ring=np.ones(blk.shape[:2],np.float32)
    bx,by,bw,bh=B[n][:4]; ring[by-1-Y0:by+bh+7-Y0, bx-1-X0:bx+bw+2-X0]=0
    # vzdialenost od tlacidla
    d=cv2.distanceTransform((ring>0).astype(np.uint8),cv2.DIST_L2,3)
    w=np.clip(1-d/R,0,1)*ring
    base=cv2.GaussianBlur(blk,(0,0),12)
    far=(d>R*0.75)&(ring>0)
    base_c=np.median(blk[far].reshape(-1,3),0)
    lowb=cv2.GaussianBlur(blk,(0,0),6); lowf=lowb.copy()
    # nizkofrekvencny presah nad zakladnu farbu panelu odobrat (detaily ostanu)
    excess=np.clip(lowb-base_c,0,None)
    corr=blk-excess*np.clip(w[...,None]*1.6,0,1)
    plate[Y0:Y1,X0:X1]=np.where(ring[...,None]>0,corr,blk)

# ---------- 4. LED ----------
for n,(x0,y0,x1,y1) in {'metro':(1188,567,1216,587),'effects':(1361,249,1378,266)}.items():
    X0,Y0,X1,Y1=x0-8,y0-8,x1+8,y1+8
    src=plate[Y0:Y1,X0:X1].copy()
    m=np.zeros(src.shape[:2],np.float32); m[8:-8,8:-8]=1; m=cv2.GaussianBlur(m,(5,5),0)
    on=src.copy()
    L_=lum(src)[...,None]; off=src*0.15+L_*np.array([0.10,0.16,0.26],np.float32)
    plate[Y0:Y1,X0:X1]=src*(1-m[...,None])+off*m[...,None]
    glow=np.zeros(src.shape[:2],np.float32); glow[4:-4,4:-4]=1; glow=cv2.GaussianBlur(glow,(9,9),0)
    save_sprite(f'led_{n}_on',on,np.maximum(m,glow*0.0))
    ctl['leds'][n]={'x':X0,'y':Y0,'w':X1-X0,'h':Y1-Y0,'on':f'assets/led_{n}_on.webp'}

# ---------- 5. VU ----------
ctl['vu']={'L':{'pivot':[279,362],'face':[178,197,388,323],'dome':[238,298,300,326]},
           'R':{'pivot':[511,352],'face':[410,190,620,318],'dome':[470,288,532,316]},
           'scaleDb':[-20,-10,-7,-5,-3,-1,0,1,2,3],'scaleX':[215,232,245,256,272,296,308,322,335,347],'length':117}
for side in ('L','R'):
    x0,y0,x1,y1=ctl['vu'][side]['dome']; src=plate[y0:y1,x0:x1]
    m=np.zeros(src.shape[:2],np.float32); g=lum(src)
    m=(g<70).astype(np.float32); m=cv2.morphologyEx(m,cv2.MORPH_OPEN,np.ones((3,3),np.uint8)); m=cv2.GaussianBlur(m,(3,3),0)
    save_sprite(f'vu_dome_{side}',src,m)
# ---------- 6. displej ----------
ctl['display']={'quad':[[662.7,159.5],[1134.2,140.6],[1162.1,358.2],[689.8,379.5]]}
cv2.imwrite('/tmp/gw/plate_final.png',np.clip(plate,0,255).astype(np.uint8))
Image.open('/tmp/gw/plate_final.png').save('/tmp/ui2/assets/panel.webp',quality=90,method=6)
json.dump(ctl,open('/tmp/ui2/controls.json','w'),indent=1)
print('ok',len(ctl['buttons']))
