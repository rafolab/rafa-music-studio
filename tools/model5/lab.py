import cv2, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
orig=cv2.imread('/tmp/g/bg0.webp.png')
def inkmask(im,box,thr=40):
    x0,y0,x1,y1=box; reg=im[y0:y1,x0:x1]
    g=cv2.cvtColor(reg,cv2.COLOR_BGR2GRAY).astype(int)
    bg=cv2.cvtColor(cv2.medianBlur(reg,21),cv2.COLOR_BGR2GRAY).astype(int)
    return ((bg-g)>thr).astype(np.uint8)
def measure(im,box):
    m=inkmask(im,box); n,lab,st,_=cv2.connectedComponentsWithStats(m)
    keep=np.zeros_like(m)
    for i in range(1,n):
        if st[i,4]>8: keep[lab==i]=1
    ys,xs=np.nonzero(keep)
    # angle: baseline = bottom-most ink per column bin
    cols=np.unique(xs); bot=np.array([ys[xs==c].max() for c in cols]); top=np.array([ys[xs==c].min() for c in cols])
    k=np.polyfit(cols,(bot+top)/2,1)[0]
    return dict(x0=xs.min()+box[0],x1=xs.max()+box[0],y0=ys.min()+box[1],y1=ys.max()+box[1],
                cx=(xs.min()+xs.max())/2+box[0],cy=(ys.min()+ys.max())/2+box[1],ang=np.degrees(np.arctan(k)),
                cap=np.median(bot-top)+1, mask=keep, box=box)
FONT='/tmp/fonts/Montserrat.ttf'
def render(word,cap,wght,track=0.0,sx=1.0):
    f=ImageFont.truetype(FONT,200); f.set_variation_by_axes([wght])
    # draw char by char for tracking
    W=sum(f.getlength(c) for c in word)+len(word)*200*track+100
    img=Image.new('L',(int(W),300),0); d=ImageDraw.Draw(img); x=20
    for c in word: d.text((x,30),c,font=f,fill=255); x+=f.getlength(c)+200*track
    img=img.crop(img.getbbox()); s=cap/ (f.getbbox('H')[3]-f.getbbox('H')[1])
    return img.resize((max(1,int(img.width*s*sx)),max(1,int(img.height*s))),Image.LANCZOS)
def ink_color(im,box):
    m=inkmask(im,box,60); x0,y0,x1,y1=box; px=im[y0:y1,x0:x1][m>0]
    return np.median(px,0)
def place(img_bgr,word,cx,cy,cap,ang,wght,track,sx,color,hl=0.0):
    g=render(word,cap,wght,track,sx)
    pad=6; G=Image.new('L',(g.width+2*pad,g.height+2*pad),0); G.paste(g,(pad,pad))
    G=G.rotate(ang,resample=Image.BICUBIC,expand=True).filter(ImageFilter.GaussianBlur(0.45))
    a=np.asarray(G).astype(np.float32)/255.0
    h,w=a.shape; px=int(round(cx-w/2)); py=int(round(cy-h/2))
    out=img_bgr.astype(np.float32)
    if hl>0:  # svetly okraj pod pismenom (vyrazene do kovu)
        ah=np.zeros_like(a); ah[1:,:]=a[:-1,:]; ah=np.clip(ah-a,0,1)*hl
        reg=out[py:py+h,px:px+w]; reg[:]=reg*(1-ah[...,None])+np.array([235,240,245])*ah[...,None]
    reg=out[py:py+h,px:px+w]; reg[:]=reg*(1-a[...,None])+np.array(color,np.float32)*a[...,None]
    return out.clip(0,255).astype(np.uint8), (px,py,w,h)
