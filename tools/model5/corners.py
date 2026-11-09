import cv2,numpy as np,json
P=cv2.imread('/mnt/user-data/uploads/c0d80071-image.png')
B={'rewind':(730,446,102,66),'stop':(840,441,101,65),'play':(950,436,103,65),'rec':(1061,430,117,72),
 'loopIn':(1231,426,86,59),'loopOut':(1326,421,88,59),'loop':(1417,416,91,68),
 'layer1':(753,594,80,61),'layer2':(837,591,74,51),'layer3':(915,588,73,50),'layer4':(992,584,73,50),'layerAdd':(1074,580,73,50),
 'metroOn':(1308,573,69,45),'mute1':(240,743,41,37),'solo1':(290,742,40,36),'mute2':(367,734,48,46),'solo2':(417,735,43,36),
 'mute3':(501,731,40,40),'solo3':(549,728,40,36),'mute4':(628,724,41,36),'solo4':(676,722,40,35)}
out={}; ov=P.copy()
for n,(x,y,w,h) in B.items():
    pad=6; X0,Y0=x-pad,y-pad; reg=P[Y0:y+h+pad+4,X0:x+w+pad]
    g=cv2.cvtColor(reg,cv2.COLOR_BGR2GRAY)
    _,m=cv2.threshold(g,0,255,cv2.THRESH_BINARY+cv2.THRESH_OTSU)
    m=cv2.morphologyEx(m,cv2.MORPH_CLOSE,np.ones((7,7),np.uint8)); m=cv2.morphologyEx(m,cv2.MORPH_OPEN,np.ones((5,5),np.uint8))
    cs,_=cv2.findContours(m,cv2.RETR_EXTERNAL,cv2.CHAIN_APPROX_SIMPLE); c=max(cs,key=cv2.contourArea)
    hull=cv2.convexHull(c).reshape(-1,2).astype(float)
    # 4 rohy: extremy x+y, x-y
    s=hull.sum(1); d=hull[:,0]-hull[:,1]
    q=np.array([hull[np.argmin(s)],hull[np.argmax(d)],hull[np.argmax(s)],hull[np.argmin(d)]])+[X0,Y0]
    out[n]=q.round(1).tolist()
    cv2.polylines(ov,[q.astype(np.int32)],True,(0,255,0),1)
json.dump(out,open('/tmp/gw/corners.json','w'))
cv2.imwrite('/tmp/gw/corners.png',cv2.resize(ov[400:800,200:1530],None,fx=1.2,fy=1.2))
