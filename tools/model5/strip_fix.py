import cv2, numpy as np, json
P=cv2.imread('/tmp/gw/plate_in.png').astype(np.float32)   # podklad pred opravou pasu
H,W=P.shape[:2]
L=P.mean(2)
# 1) oblast tmaveho panelu kanalov: suvisla tmava plocha okolo (450,620)
lf=cv2.GaussianBlur(L,(0,0),3)
dark=(lf<75).astype(np.uint8)
dark[:380,:]=0; dark[:,770:]=0; dark[820:,:]=0
n,lab,st,_=cv2.connectedComponentsWithStats(dark)
reg=(lab==lab[620,450]).astype(np.uint8)
reg=cv2.morphologyEx(reg,cv2.MORPH_CLOSE,np.ones((41,41),np.uint8))   # zaplnit dierky (znacky, gombiky)
reg=cv2.erode(reg,np.ones((9,9),np.uint8))
# 2) vynechat prvky: svetle znacky, gombiky PAN, tlacidla M/S, drazky
feat=np.zeros_like(reg)
hp=L-cv2.GaussianBlur(L,(0,0),6)
feat|=(hp>10).astype(np.uint8)            # svetle znacky
feat|=(L<12).astype(np.uint8)             # drazky
kn=json.load(open('/tmp/gw/knobs.json'))
for k in ('ch1','ch2','ch3','ch4'):
    c=kn[k]; cv2.circle(feat,(int(c['cx']),int(c['cy'])),46,1,-1)
ctl=json.load(open('/tmp/ui2/controls.json'))
for n_,b in ctl['buttons'].items():
    if n_.startswith(('mute','solo')): cv2.rectangle(feat,(b['x']-8,b['y']-8),(b['x']+b['w']+8,b['y']+b['h']+8),1,-1)
feat=cv2.dilate(feat,np.ones((5,5),np.uint8))
samp=(reg>0)&(feat==0)
# 3) nizkofrekvencna zlozka (normalizovana konvolucia cez vzorky)
def nconv(img,m,s):
    num=cv2.GaussianBlur(img*m,(0,0),s); den=cv2.GaussianBlur(m.astype(np.float32),(0,0),s)
    return num/np.maximum(den,1e-4)
m=samp.astype(np.float32)
LF=np.dstack([nconv(P[...,c],m,4) for c in range(3)])
# 4) hladka plocha: polynom 3. stupna z cistych vzoriek
ys,xs=np.nonzero(samp); sel=np.random.default_rng(0).choice(len(xs),min(40000,len(xs)),replace=False); xs,ys=xs[sel],ys[sel]
def basis(x,y):
    x=(x-450)/300.; y=(y-600)/220.
    return np.stack([np.ones_like(x),x,y,x*x,x*y,y*y,x**3,x*x*y,x*y*y,y**3],-1)
A=basis(xs.astype(float),ys.astype(float))
YY,XX=np.mgrid[0:H,0:W]
fit=np.zeros_like(P)
for c in range(3):
    coef,*_=np.linalg.lstsq(A,LF[ys,xs,c],rcond=None)
    fit[...,c]=(basis(XX.astype(float),YY.astype(float))@coef)
# 5) nova plocha = povodne detaily (vysoke frekvencie) + hladke osvetlenie
# rovnaka textura vsade: mimo prvkov (znacky, drazky) len hladka plocha + jednotne jemne zrno
featsoft=cv2.GaussianBlur(cv2.dilate(feat,np.ones((3,3),np.uint8)).astype(np.float32),(0,0),1.2)[...,None]
grain=cv2.GaussianBlur(np.random.default_rng(7).normal(0,1.6,(H,W)).astype(np.float32),(0,0),0.7)[...,None]
out=(P-LF+fit)*featsoft+(fit+grain)*(1-featsoft)
# gombiky a M/S ponechat povodne (makky prechod), aplikovat len v oblasti panelu
keep=np.zeros((H,W),np.float32)
for k in ('ch1','ch2','ch3','ch4'):
    c=kn[k]; cv2.circle(keep,(int(c['cx']),int(c['cy'])),40,1,-1)
slot=cv2.dilate((cv2.GaussianBlur(L,(0,0),1)<12).astype(np.uint8),np.ones((5,5),np.uint8)).astype(np.float32)
keep=np.maximum(keep,slot*reg)
for n_,b in ctl['buttons'].items():
    if n_.startswith(('mute','solo')): cv2.rectangle(keep,(b['x']-3,b['y']-3),(b['x']+b['w']+3,b['y']+b['h']+3),1,-1)
keep=cv2.GaussianBlur(keep,(0,0),3)
area=cv2.GaussianBlur(reg.astype(np.float32),(0,0),4)
w=(area*(1-np.clip(keep,0,1)))[...,None]
res=np.clip(P*(1-w)+out*w,0,255)
cv2.imwrite('/tmp/gw/plate_strip.png',res.astype(np.uint8))
g=res.mean(2)[370:820,130:770]; e=np.clip((g-20)*5,0,255).astype(np.uint8)
cv2.imwrite('/tmp/gw/strip_contrast2.png',cv2.resize(e,None,fx=1.4,fy=1.4))
cv2.imwrite('/tmp/gw/strip_new.png',cv2.resize(res[370:820,130:770].astype(np.uint8),None,fx=1.4,fy=1.4))
dbg=np.dstack([reg*255, samp.astype(np.uint8)*255, (w[...,0]*255).astype(np.uint8)])[370:820,130:770]
cv2.imwrite('/tmp/gw/strip_dbg.png',cv2.resize(dbg,None,fx=1.4,fy=1.4))
