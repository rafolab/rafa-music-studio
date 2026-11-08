import cv2,numpy as np
o=cv2.imread('/tmp/gw/plate_vu.png')
Q=np.array([[662.7,159.5],[1134.2,140.6],[1162.1,358.2],[689.8,379.5]])
c=Q.mean(0); Qi=c+(Q-c)*np.array([[0.985,0.97]])  # trochu dovnutra, nechat okraj skla
mask=np.zeros(o.shape[:2],np.uint8); cv2.fillPoly(mask,[np.round(Qi).astype(np.int32)],255)
orig=cv2.imread('/mnt/user-data/uploads/c0d80071-image.png')
hsv=cv2.cvtColor(orig,cv2.COLOR_BGR2HSV)
content=((hsv[...,1]>80)&(hsv[...,2]>60)).astype(np.uint8)
content=cv2.dilate(content,np.ones((7,7),np.uint8))
glass=(mask>0)&(content==0)
ys,xs=np.nonzero(glass); vals=orig[ys,xs].astype(float)
# rovina farby skla: c = a + b*x + c*y (jemny prechod)
A=np.c_[np.ones(len(xs)),xs,ys]; coef=np.linalg.lstsq(A,vals,rcond=None)[0]
YY,XX=np.mgrid[0:o.shape[0],0:o.shape[1]]
fill=(coef[0][None,None,:]+XX[...,None]*coef[1]+YY[...,None]*coef[2])
noise=np.random.default_rng(1).normal(0,0.8,fill.shape)
fill=np.clip(fill+noise,0,255)
soft=cv2.GaussianBlur(mask.astype(np.float32)/255,(5,5),0)[...,None]
out=(o*(1-soft)+fill*soft).astype(np.uint8)
cv2.imwrite('/tmp/gw/plate_step1.png',out)
cv2.imwrite('/tmp/gw/glass2.png',cv2.resize(out[120:400,640:1190],None,fx=1.5,fy=1.5))
print(coef)
