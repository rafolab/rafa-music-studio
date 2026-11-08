import cv2,numpy as np
o=cv2.imread('/tmp/gw/plate_labels.png')
g=cv2.cvtColor(o,cv2.COLOR_BGR2GRAY).astype(int)
bg=cv2.medianBlur(o,21); bgg=cv2.cvtColor(bg,cv2.COLOR_BGR2GRAY).astype(int)
mask=np.zeros(g.shape,np.uint8)
for (x0,y0,x1,y1) in [(216,265,263,316),(448,255,498,305)]:
    d=np.array([x1-x0,y1-y0],float); d/=np.linalg.norm(d)
    tip=np.array([x0,y0])-d*13
    t=(306-y0)/d[1]; end=np.array([x0,y0])+d*t
    lm=np.zeros_like(mask); cv2.line(lm,tuple(np.round(tip).astype(int)),tuple(np.round(end).astype(int)),255,5)
    dark=((bgg-g)>12).astype(np.uint8)*255
    mask|=cv2.dilate(lm&dark,np.ones((3,3),np.uint8),iterations=2)
    wide=np.zeros_like(mask); mid=np.array([x0,y0])+d*((282-y0)/d[1])
    cv2.line(wide,tuple(np.round(mid).astype(int)),tuple(np.round(end).astype(int)),255,10); mask|=wide
o2=cv2.inpaint(o,mask,5,cv2.INPAINT_TELEA)
cv2.imwrite('/tmp/gw/plate_vu.png',o2)
cv2.imwrite('/tmp/gw/vu2.png',cv2.resize(o2[185:330,165:625],None,fx=2,fy=2,interpolation=cv2.INTER_LANCZOS4))
