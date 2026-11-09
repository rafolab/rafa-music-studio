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
from PIL import ImageFont, ImageDraw
FONT=ImageFont.truetype('/tmp/fonts/Montserrat.ttf',60); FONT.set_variation_by_axes([550])
def slotfun(f):
    (tx,ty),(bx,by)=f['top'],f['bot']; sl=(bx-tx)/(by-ty); return (lambda y: tx+(y-ty)*sl), sl
def draw_text_alpha(text,cap):
    img=Image.new('L',(400,120),0); d=ImageDraw.Draw(img); d.text((10,10),text,font=FONT,fill=255)
    img=img.crop(img.getbbox()); hb=FONT.getbbox('0'); s_=cap/(hb[3]-hb[1])
    return np.asarray(img.resize((max(1,int(img.width*s_)),max(1,int(img.height*s_))),Image.LANCZOS)).astype(np.float32)/255
def stamp(img,alpha,x,y,color):
    h,w=alpha.shape; x=int(round(x)); y=int(round(y))
    img[y:y+h,x:x+w]=img[y:y+h,x:x+w]*(1-alpha[...,None])+np.array(color,np.float32)*alpha[...,None]
def hline(img,y,xa,xb,color,th=1.6):
    # vodorovna ryzka s antialiasingom
    for yy in range(int(np.floor(y-th)),int(np.ceil(y+th))+1):
        a_=np.clip(th/2+0.5-abs(yy-y),0,1)
        if a_<=0: continue
        xs=np.arange(int(np.floor(xa)),int(np.ceil(xb))+1)
        ax=np.clip(np.minimum(xs-xa+1,xb-xs+1),0,1)*a_
        img[yy,xs]=img[yy,xs]*(1-ax[:,None])+np.array(color,np.float32)*ax[:,None]
# spolocne parametre z cistych miest povodneho obrazka
prof=[]
for n,f in F.items():
    slotx,_=slotfun(f); x0,y0,x1,y1=f['cap']; (tx,ty),(bx,by)=f['top'],f['bot']
    for y in range(int(ty+20),int(by-20)):
        if y0-35<=y<=y1+45: continue
        cx=slotx(y); xs=np.arange(-12,13)
        row=np.array([orig_plate[y,int(np.floor(cx+d))]*(1-((cx+d)%1))+orig_plate[y,int(np.floor(cx+d))+1]*((cx+d)%1) for d in xs])
        prof.append(row)
prof=np.median(np.array(prof),0)            # priečny rez drážkou (25 bodov)
tickcol=np.array([205,208,212],np.float32)  # farba ryziek (BGR)
labcol=np.array([214,216,220],np.float32)
DIV=[(182,210),(312,342),(438,473),(563,602),(695,740)]
divx=lambda i,y: DIV[i][0]+(DIV[i][1]-DIV[i][0])*(y-500)/220
for n,f in F.items():
    slotx,slant=slotfun(f); (tx,ty),(bx,by)=f['top'],f['bot']; x0,y0,x1,y1=f['cap']
    Y0,Y1=int(ty-16),int(by+16)
    fi=int(n[1])-1
    XL=lambda y: int(round(divx(fi,y)+4)); XR=lambda y: int(round(divx(fi+1,y)-3))
    # 1) podklad panelu: hladky priebeh farby z cisteho stlpca vpravo od ryziek
    ys=[];cs=[]
    for y in range(Y0,Y1):
        if y0-35<=y<=y1+50: continue
        xr=XR(y)-12; ys.append(y); cs.append(np.median(orig_plate[y,xr:xr+8].reshape(-1,3),0))
    ys=np.array(ys); cs=np.array(cs)
    coef=[np.polyfit(ys,cs[:,k],2) for k in range(3)]
    rng=np.random.default_rng(int(n[1]))
    for y in range(Y0,Y1):
        xl=XL(y); xr=XR(y)
        base=np.array([np.polyval(coef[k],y) for k in range(3)],np.float32)
        plate[y,xl:xr]=base+rng.normal(0,1.1,(xr-xl,1)).astype(np.float32)
    # mäkký prechod na okrajoch oblasti do povodneho
    # 2) drážka (kapsula) podla priecneho rezu
    R_=7.0
    for y in range(Y0,Y1):
        cx=slotx(y)
        dt=min(y-(ty-6), (by+6)-y)
        if dt<0: continue
        k_=1.0 if dt>=R_ else np.sqrt(max(0,1-((R_-dt)/R_)**2))
        xs_=np.arange(int(np.floor(cx-14)),int(np.ceil(cx+15)))
        dd=(xs_-cx)/max(k_,1e-3)
        col=np.stack([np.interp(dd,np.arange(-12,13),prof[:,c_]) for c_ in range(3)],1)
        w_=np.clip(12.5-np.abs(dd),0,1)*(1.0 if k_>0.05 else 0)
        plate[y,xs_]=plate[y,xs_]*(1-w_[:,None])+col*w_[:,None]
    # 3) ryzky: 4 dieliky medzi znackami; vlavo kratke (pri znackach dlhsie), vpravo dlhe
    mk=[f['marks'][6],f['marks'][0],f['marks'][-12],f['marks'][-100]]
    ticks=[]
    for i in range(3):
        for j in range(4): ticks.append((mk[i]+(mk[i+1]-mk[i])*j/4, j==0))
    ticks.append((mk[3],True))
    for (y,major) in ticks:
        cx=slotx(y)
        hline(plate,y+1,cx-(25 if major else 21),cx-11,tickcol,1.5)
        hline(plate,y-2,cx+12,cx+(31 if major else 28),tickcol,1.5)
    # 4) napisy +6, 0, -12, -∞ (zarovnane vpravo pred ryzkou)
    for txt,y in (('+6',mk[0]),('0',mk[1]),('-12',mk[2]),('-∞',mk[3])):
        al=draw_text_alpha(txt.replace('-','−'),9.5)
        stamp(plate,al,slotx(y)-30-al.shape[1],y-al.shape[0]/2+0.5,labcol)
    # mäkké okraje oblasti
for n,f in F.items():
    slotx,_=slotfun(f); (tx,ty),(bx,by)=f['top'],f['bot']
    Y0,Y1=int(ty-16),int(by+16)
    fi=int(n[1])-1
    XL=lambda y: int(round(divx(fi,y)+4)); XR=lambda y: int(round(divx(fi+1,y)-3))
    for y in list(range(Y0,Y0+4))+list(range(Y1-4,Y1)):
        w_=(min(y-Y0,Y1-1-y)+0.5)/4
        xl=XL(y); xr=XR(y)
        plate[y,xl:xr]=orig_plate[y,xl:xr]*(1-w_)+plate[y,xl:xr]*w_
    for y in range(Y0,Y1):
        xl=XL(y); xr=XR(y)
        for k in range(2):
            w_=(k+0.5)/2
            plate[y,xl+k]=orig_plate[y,xl+k]*(1-w_)+plate[y,xl+k]*w_
            plate[y,xr-1-k]=orig_plate[y,xr-1-k]*(1-w_)+plate[y,xr-1-k]*w_
# 5) sprity hlaviciek: plna hlavicka + mäkký tien (len tmavsie miesta, vyhladene)
for n,f in F.items():
    slotx,slant=slotfun(f); x0,y0,x1,y1=f['cap']
    S=(x0-8,y0-10,x1+12,y1+20)
    o=orig_plate[S[1]:S[3],S[0]:S[2]]; r=plate[S[1]:S[3],S[0]:S[2]]
    inner=np.zeros(o.shape[:2],np.float32); inner[(y0-S[1])+1:(y1-S[1]),(x0-S[0])+1:(x1-S[0])-1]=1
    edge=cv2.dilate(inner,np.ones((5,5),np.uint8))
    d=np.abs(o-r).max(2); al_edge=np.clip((d-14)/30,0,1)*edge
    dark=np.clip((lum(r)-lum(o)),0,None); dark=cv2.GaussianBlur(dark,(0,0),3)
    shadow=np.clip(dark/max(1,lum(r).mean()),0,0.85)
    ring=cv2.GaussianBlur(cv2.dilate(inner,np.ones((21,21),np.uint8)),(0,0),3)
    al=np.maximum(np.maximum(inner,al_edge),shadow*ring)
    col=np.where(inner[...,None]>0.5,o,o*0+np.where((al_edge>0)[...,None],o,0))
    # jazdec bez farebneho nadychu od povodne svietiaceho M (fader 2 bol oranzovy)
    oL=lum(o)[...,None]; o=o*0.35+np.repeat(oL,3,2)*0.65
    save_sprite(f'fader_{n}',np.where((inner+al_edge)[...,None]>0,o,np.zeros_like(o)),al)
    ctl['faders'][n]={'sprite':f'assets/fader_{n}.webp','x':S[0],'y':S[1],'w':S[2]-S[0],'h':S[3]-S[1],
        'grooveY':f['groove'],'slant':round(slant,4),'marks':{str(k):v for k,v in f['marks'].items()},
        'track':int(n[1])-1}
cv2.imwrite('/tmp/gw/plate_nofaders.png',plate.astype(np.uint8))
json.dump(ctl,open('/tmp/gw/ctl_partial.json','w'),indent=1)

# ---------- 2b. pas kanalov PAN/fadery: jednotna plocha (Etapa 2E) ----------
cv2.imwrite('/tmp/gw/plate_in.png',np.clip(plate,0,255).astype(np.uint8))
import subprocess; subprocess.run(['python3','/tmp/gw/strip_fix.py'],check=True)
plate=cv2.imread('/tmp/gw/plate_strip.png').astype(np.float32)
# ---------- 3. tlacidla ----------
B={ # id: (x,y,w,h) z detekcie, typ
 'project':(1330,148,113,41,'menu'),'tracks':(1336,196,115,41,'menu'),'effects':(1342,245,117,42,'menu'),'settings':(1348,292,119,38,'menu'),
 'rewind':(730,446,102,66,'grey'),'stop':(840,441,101,65,'grey'),'play':(950,436,103,65,'grey'),'rec':(1061,430,117,72,'rec'),
 'loopIn':(1231,426,86,59,'grey'),'loopOut':(1326,421,88,59,'grey'),'loop':(1417,416,91,68,'litgrey'),
 'layer1':(753,594,80,61,'litgrey'),'layer2':(837,591,74,51,'grey'),'layer3':(915,588,73,50,'grey'),'layer4':(992,584,73,50,'grey'),'layerAdd':(1074,580,73,50,'grey'),
 'metroOn':(1308,573,69,45,'litgrey'),
 'mute1':(240,743,41,37,'litcream'),'solo1':(290,742,40,36,'grey2'),'mute2':(369,737,41,37,'litcream'),'solo2':(417,735,43,36,'grey2fix'),
 'mute3':(501,731,40,40,'cream'),'solo3':(549,728,40,36,'grey2'),'mute4':(628,724,41,36,'litcream'),'solo4':(676,722,40,35,'grey2fix'),
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
def glyph_alpha(src,sig=5):
    L=lum(src); bl=cv2.GaussianBlur(L,(0,0),sig)
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
# ===== Etapa 2E: vsetky tlacidla z jedneho cisteho vzoru na rad + jednotny svetelny model =====
MASTER={'rewind':'stop','stop':'stop','play':'stop','rec':'stop',
        'loopIn':'loopOut','loopOut':'loopOut','loop':'loopOut',
        'layer1':'layer3','layer2':'layer3','layer3':'layer3','layer4':'layer3','layerAdd':'layer3','metroOn':'layer3',
        'mute1':'mute3','mute2':'mute3','mute3':'mute3','mute4':'mute3',
        'solo1':'solo3','solo2':'solo3','solo3':'solo3','solo4':'solo3'}
QUAD=json.load(open('/tmp/gw/corners.json'))
def fix_quads():
    good=[k for k in QUAD if k not in ('rec','layer2','solo2')]
    # priemerny sklon hornej a lavej hrany z dobrych tlacidiel
    tops=[(QUAD[k][1][1]-QUAD[k][0][1])/(QUAD[k][1][0]-QUAD[k][0][0]) for k in good]
    lefts=[(QUAD[k][3][0]-QUAD[k][0][0])/(QUAD[k][3][1]-QUAD[k][0][1]) for k in good]
    ts,ls=float(np.median(tops)),float(np.median(lefts))
    # chybne rohy: tvar podobneho susedneho tlacidla, prenesený podla obdlznika
    for k,ref in (('rec','play'),('layer2','layer3'),('solo2','solo1')):
        rx,ry,rw,rh=B[ref][:4]; x,y,w,h=B[k][:4]
        QUAD[k]=[[x+(px-rx)*w/rw, y+(py-ry)*h/rh] for px,py in QUAD[ref]]
fix_quads()
def fit_row(row,good):
    A=np.array([QUAD[row[i]] for i in good],np.float32)
    for i,k in enumerate(row):
        QUAD[k]=[[float(np.polyval(np.polyfit(good,A[:,c,d],1),i)) for d in range(2)] for c in range(4)]
fit_row(['layer1','layer2','layer3','layer4','layerAdd'],[2,3,4])
LIT1Q=[list(p) for p in QUAD['layer1']]
fit_row(['mute1','mute2','mute3','mute4'],[0,2,3]); fit_row(['solo1','solo2','solo3','solo4'],[0,2,3])
# --- tlacidlo 1: povodne svieti (vacsie, zatlacene) -> svietiacu predlohu si odlozit, na podklade ho
#     nahradit klonom zhasnuteho tlacidla 2 a dat mu rovnaku velkost ako 2,3,4,+
OLD1=B['layer1']; FACE1=clean('layer1'); GA1=glyph_alpha(region('layer1')); OFF1=crop(OLD1)[:2]
dx,dy=-78,3
for arr in (P,plate):   # lavy okraj povodne svietiaceho 1: bez oranzoveho svetla, tmavsi
    st=arr[570:676,726:770].copy(); mk_=np.zeros(st.shape[:2],np.uint8); mk_[14:96,10:32]=255
    inp=cv2.inpaint(np.clip(st,0,255).astype(np.uint8),mk_,6,cv2.INPAINT_TELEA).astype(np.float32)
    fm=cv2.GaussianBlur(mk_.astype(np.float32)/255,(0,0),2)[...,None]
    arr[570:676,726:770]=inp*fm+st*(1-fm)
X0,Y0,X1,Y1=752,578,836,668
cm=np.zeros((Y1-Y0,X1-X0),np.float32); cm[4:-4,3:-4]=1; cm=cv2.GaussianBlur(cm,(0,0),2)[...,None]
srcblk=P[Y0-dy:Y1-dy,X0-dx:X1-dx].copy()
for arr in (P,plate):
    arr[Y0:Y1,X0:X1]=srcblk*cm+arr[Y0:Y1,X0:X1]*(1-cm)
B['layer1']=(837+dx,591+dy,74,51,'grey')
QUAD['layer1']=[[x+dx,y+dy] for x,y in QUAD['layer2']]
QUAD['_lit1']=None
def lit_persp(n,glyph,ink_col,master='layer1'):
    """svietiaci stav: prava fotka svietiaceho '1' prenesena homografiou z presnych rohov '1'
    na presne rohy tlacidla n (rovnaka perspektiva konzoly), maska = vlastny tvar tlacidla n"""
    mx0,my0=OFF1; x0,y0,x1,y1=crop(B[n]); sz=(x1-x0,y1-y0)
    face=FACE1
    S=np.float32(LIT1Q)-np.float32([mx0,my0]); D=np.float32(QUAD[n])-np.float32([x0,y0])
    Hm=cv2.getPerspectiveTransform(S,D)
    w=cv2.warpPerspective(face,Hm,sz,flags=cv2.INTER_CUBIC,borderMode=cv2.BORDER_REPLICATE)
    # maska: horna plocha + predna hrana (o ~13 % vysky nizsie), vsetko v perspektive tlacidla n
    q=D.copy(); hgt=((q[3][1]-q[0][1])+(q[2][1]-q[1][1]))/2
    ext=np.float32([q[0]+[-2,-2],q[1]+[2,-2],q[2]+[2,hgt*0.14],q[3]+[-2,hgt*0.14]])
    mk=np.zeros((sz[1],sz[0]),np.uint8); cv2.fillConvexPoly(mk,np.round(ext).astype(np.int32),255)
    mk=cv2.GaussianBlur(mk.astype(np.float32)/255,(0,0),1.2)[...,None]
    base=warp_master(MASTER[n],n)
    out=w*mk+base*(1-mk)
    return np.clip(out*(1-glyph[...,None])+ink_col*glyph[...,None],0,255)
def lit_ms(n,glyph,kind):
    x0,y0,x1,y1=crop(B[n]); base=warp_master(MASTER[n],n)
    full=lit_persp(n,np.zeros_like(glyph),np.zeros(3,np.float32))
    d=np.abs(full-base).sum(2,keepdims=True); lm=np.clip(d/40,0,1)      # kde je svetlo
    if kind=='M': mul=np.array([0.78,0.40,1.0],np.float32)   # jantar -> svietiaca cervena (BGR)
    else:         mul=np.array([0.70,1.10,1.0],np.float32)   # jantar -> svietiaca zlta
    col=np.clip(full*mul,0,255); col=col*lm+base*(1-lm)
    ink_=np.array([40,40,90],np.float32) if kind=='M' else np.array([30,60,90],np.float32)
    return np.clip(col*(1-glyph[...,None])+ink_*glyph[...,None],0,255)
def homo_warp(img_src,src_name,dst_name,offset_src,offset_dst,size):
    S=np.float32(QUAD[src_name])-np.float32(offset_src); D=np.float32(QUAD[dst_name])-np.float32(offset_dst)
    Hm=cv2.getPerspectiveTransform(S,D)
    return cv2.warpPerspective(img_src,Hm,size,flags=cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE)
def lit_from(master,n,glyph,ink_col,tint=None,affine=False):
    """svietiaci stav z pravej fotky svietiaceho tlacidla, natvarovany do perspektivy tlacidla n"""
    mx0,my0,_,_=crop(B[master]); x0,y0,x1,y1=crop(B[n])
    face=clean(master)
    if tint is not None:
        L=lum(face)/255.0; h_,w_=L.shape
        face=np.clip((0.15+1.05*L)[...,None]*tint,0,255)
    # len telo tlacidla z predlohy (bez susednych tlacidiel v okraji)
    bx,by,bw,bh=B[master][:4]
    mm=np.zeros(face.shape[:2],np.float32); mm[by-my0-1:by-my0+bh+7,bx-mx0-1:bx-mx0+bw+2]=1
    if affine:
        rb=B[master]; bw_,bh_=B[n][2],B[n][3]; sx_=bw_/rb[2]; sy_=bh_/rb[3]
        M=np.float32([[sx_,0,4-4*sx_],[0,sy_,4-4*sy_]]); sz=(x1-x0,y1-y0)
        w=cv2.warpAffine(face,M,sz,flags=cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE)
        wm=cv2.warpAffine(mm,M,sz,flags=cv2.INTER_LINEAR)
    else:
        w=homo_warp(face,master,n,(mx0,my0),(x0,y0),(x1-x0,y1-y0))
        wm=homo_warp(mm,master,n,(mx0,my0),(x0,y0),(x1-x0,y1-y0))
    wm=cv2.GaussianBlur(np.clip(wm,0,1),(3,3),0)[...,None]
    base=warp_master(MASTER[n],n)
    w=w*wm+base*(1-wm)
    return w*(1-glyph[...,None])+ink_col*glyph[...,None]
LIT_PAL={}
def lit_own(n,base,glyph,ink_col,master='layer1'):
    """svietiaci stav z TVARU tohto tlacidla: jas kazdeho bodu sa prevedie na farby pravej svietiacej
    predlohy (kvantilove mapovanie) a prida sa jemny priebeh svetla predlohy (len nizke frekvencie)"""
    if master not in LIT_PAL: LIT_PAL[master]=core(clean(master))
    # ryhy brúseného kovu pod svetlom zjemnit (kryt je mliecny), tvar a hrany ostanu
    sm=cv2.bilateralFilter(np.clip(base,0,255).astype(np.uint8),11,26,11).astype(np.float32)
    col=remap(sm,LIT_PAL[master])
    # priebeh svetla z predlohy (rozmazany), prenesený len zvacsenim
    rb=B[master]; bw_,bh_=B[n][2],B[n][3]; sx_=bw_/rb[2]; sy_=bh_/rb[3]
    mf=clean(master); Lm=cv2.GaussianBlur(lum(mf),(0,0),6)
    x0,y0,x1,y1=crop(B[n]); M=np.float32([[sx_,0,4-4*sx_],[0,sy_,4-4*sy_]])
    Lw=cv2.warpAffine(Lm,M,(x1-x0,y1-y0),borderMode=cv2.BORDER_REPLICATE)
    h_,w_=Lw.shape; cm=np.zeros_like(Lw,bool); cm[6:h_-12,6:w_-6]=True
    g=np.clip(Lw/max(1,np.median(Lw[cm])),0.75,1.2)[...,None]
    Lb=lum(base); gb=np.clip(cv2.GaussianBlur(Lb,(0,0),6)/max(1,np.median(Lb[cm])),0.75,1.2)[...,None]
    col=np.clip(col*(g/gb)**0.6,0,255)
    # farebne zladit s pravym svietiacim tlacidlom (median farby tela)
    ref=np.median(LIT_PAL[master],0); cur=np.median(col[cm],0)
    col=np.clip(col*(ref/np.maximum(cur,1)),0,255)
    # brusena textura kovu cez svetlo len jemne (pod svetlom je mlecny kryt)
    col=cv2.bilateralFilter(col.astype(np.float32),5,18,5)
    return col*(1-glyph[...,None])+ink_col*glyph[...,None]
def warp_master(master,n):
    rb=B[master]; bx,by,bw,bh=B[n][:4]; x0,y0,x1,y1=crop(B[n])
    body=clean(master)
    # neutralizovat farebny nadych vzoru (bez oranzovych odleskov), zachovat tienovanie
    Lb=lum(body)[...,None]; gray=np.repeat(Lb,3,2)
    tint=np.median(core(body),0); tint=tint/max(1,tint.mean())
    body=gray*0.55+body*0.45
    sx_=bw/rb[2]; sy_=bh/rb[3]
    M=np.float32([[sx_,0,4-4*sx_],[0,sy_,4-4*sy_]])
    return cv2.warpAffine(body,M,(x1-x0,y1-y0),flags=cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE)
AMBER=np.median(core(clean('layer1')),0)            # farba povodneho svietiaceho tlacidla
AMBER=AMBER/AMBER.max()*np.array([1,1,1])*255*np.array([0.42,0.72,1.0])/np.array([0.42,0.72,1.0]).max()
AMBER=np.array([42,160,255],np.float32)             # jantar (BGR) – rovnaky pre vsetky tlacidla
RED_M=np.array([52,72,236],np.float32)               # M: tepla cervena
YEL_S=np.array([62,200,250],np.float32)              # S: jantarovo-zlta
INK_OFF=ink('layer3'); INK_ON=np.array([24,40,92],np.float32)
INK_LIT=ink('layer1')
LIT_MASTER={'loopIn':'loop','loopOut':'loop','rewind':'loop','stop':'loop','play':'loop'}
SOFT_RED=np.array([72,82,196],np.float32)    # M: zjemnena cervena (BGR)
SOFT_YEL=np.array([78,178,222],np.float32)   # S: zjemnena jantarovo-zlta
def light(base,colr,lift=0.0):
    # svetlo zvnutra: tienovanie tlacidla sa zachova, jas sa zjednoti
    L=lum(base); h,w=L.shape; cm=np.zeros_like(L,bool); cm[6:h-12,6:w-6]=True
    n_=L/max(1,np.median(L[cm]))
    k=np.clip(0.50+0.55*n_+lift,0,1.25)[...,None]
    # jemna ziara stredu
    yy,xx=np.mgrid[0:h,0:w]; g=np.exp(-(((xx-w/2)/(w*0.55))**2+((yy-h*0.42)/(h*0.55))**2))[...,None]
    vg=np.linspace(1.04,0.9,h)[:,None,None]
    return np.clip(colr*k*(0.92+0.14*g)*vg,0,255)
for n,r in B.items():
    x0,y0,x1,y1=crop(r); src=P[y0:y1,x0:x1].copy(); kind=r[4]
    bx,by,bw,bh=r[:4]
    m=np.zeros(src.shape[:2],np.float32); m[4-1:4+bh+7,4-1:4+bw+2]=1; m=cv2.GaussianBlur(m,(3,3),0)
    if kind=='menu':
        off=src; L=lum(src); t=np.clip((L-90)/80,0,1)[...,None]
        on=src*(1-t)+np.array([90,190,255],np.float32)*t
    elif kind=='rec':
        on=src
        L=lum(src); bl=cv2.GaussianBlur(L,(0,0),5)
        gb=np.clip((L-bl-12)/30,0,1)
        inner=np.zeros_like(gb); h_,w_=gb.shape; inner[int(h_*0.2):int(h_*0.66),int(w_*0.15):int(w_*0.85)]=1
        gb=cv2.GaussianBlur(gb*inner,(3,3),0)
        base=warp_master('stop',n)
        off=base*(1-gb[...,None])+np.array([38,44,196],np.float32)*gb[...,None]
    else:
        base=warp_master(MASTER[n],n)
        ga=glyph_alpha(src,9 if n=='stop' else 5)    # znak tlacidla z originalu (na presnom mieste)
        if n=='layer1':   # znak '1' z povodnej fotky, preneseny do novej (zhodnej) polohy
            Sq=np.float32(LIT1Q)-np.float32(OFF1); Dq=np.float32(QUAD[n])-np.float32([x0,y0])
            ga=cv2.warpPerspective(GA1,cv2.getPerspectiveTransform(Sq,Dq),(x1-x0,y1-y0),flags=cv2.INTER_LINEAR)
        if n.startswith('mute'):
            Lo=lum(base)[...,None]; base=base*0.62+Lo*0.38; base=base*0.93
            off=base*(1-ga[...,None])+ink('mute3')*ga[...,None]
            on=lit_ms(n,ga,'M')
        elif n.startswith('solo'):
            off=base*(1-ga[...,None])+ink('solo3')*ga[...,None]
            on=lit_ms(n,ga,'S')
        else:
            off=base*(1-ga[...,None])+INK_OFF*ga[...,None]
            if n in ('loop','metroOn'): on=src          # povodne svietiace tlacidla z fotky
            else: on=lit_persp(n,ga,INK_LIT)
    off=np.where(m[...,None]>0,off,src); on=np.where(m[...,None]>0,on,src)
    plate[y0:y1,x0:x1]=src*(1-m[...,None])+off*m[...,None]
    save_sprite(f'btn_{n}_off',off,m); save_sprite(f'btn_{n}_on',on,m)
    ctl['buttons'][n]={'x':x0,'y':y0,'w':x1-x0,'h':y1-y0,'off':f'assets/btn_{n}_off.webp','on':f'assets/btn_{n}_on.webp','kind':kind}

# svatozar okolo povodne svietiacich tlacidiel: ubrat oranzovy presah
for n in ('layer1','layer2','mute1','mute2','mute4','solo4','loop','metroOn','rec','loopIn'):
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
    # zvysny farebny nadych (oranzovy / cerveny) odobrat smerom k sedej
    gl=lum(corr)[...,None]; ww=np.clip(w[...,None]*1.8,0,1)*0.85
    corr=corr*(1-ww)+np.repeat(gl,3,2)*ww
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
# napis PHONES odstranit (ikona sluchadiel a konektor ostavaju)
_X0,_Y0,_X1,_Y1=1436,654,1536,682
_blk=plate[_Y0:_Y1,_X0:_X1]; _L=lum(_blk); _bl=cv2.GaussianBlur(_L,(0,0),6)
_mk=((_bl-_L)>18).astype(np.uint8); _mk=cv2.dilate(_mk,np.ones((3,3),np.uint8),iterations=2)
_inp=cv2.inpaint(np.clip(_blk,0,255).astype(np.uint8),_mk*255,5,cv2.INPAINT_TELEA).astype(np.float32)
_sh=plate[_Y0-24:_Y1-24,_X0:_X1]; _hf=_sh-cv2.GaussianBlur(_sh,(0,0),2)
_fill=cv2.GaussianBlur(_inp,(0,0),2)+_hf
_m=cv2.GaussianBlur(_mk.astype(np.float32),(0,0),1)[...,None]
plate[_Y0:_Y1,_X0:_X1]=_fill*_m+_blk*(1-_m)
cv2.imwrite('/tmp/gw/plate_final.png',np.clip(plate,0,255).astype(np.uint8))
Image.open('/tmp/gw/plate_final.png').save('/tmp/ui2/assets/panel.webp',quality=90,method=6)
json.dump(ctl,open('/tmp/ui2/controls.json','w'),indent=1)
print('ok',len(ctl['buttons']))
