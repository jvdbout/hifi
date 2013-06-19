//
//  Webcam.h
//  interface
//
//  Created by Andrzej Kapolka on 6/17/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__Webcam__
#define __interface__Webcam__

#include <QMetaType>
#include <QObject>
#include <QThread>

#include <opencv2/opencv.hpp>

#include "InterfaceConfig.h"

class QImage;

struct CvCapture;

class FrameGrabber;

class Webcam : public QObject {
    Q_OBJECT
    
public:
    
    Webcam();
    ~Webcam();

    void renderPreview(int screenWidth, int screenHeight);

public slots:
    
    void setEnabled(bool enabled);
    void setFrame(const cv::Mat& image);
    
private:
    
    QThread _grabberThread;
    FrameGrabber* _grabber;
    
    bool _enabled;
    int _frameWidth;
    int _frameHeight;
    GLuint _frameTextureID;
    
    long long _startTimestamp;
    int _frameCount;
    
    long long _lastFrameTimestamp;
};

class FrameGrabber : public QObject {
    Q_OBJECT
    
public:
    
    FrameGrabber() : _capture(0) { }
    virtual ~FrameGrabber();

public slots:
    
    void grabFrame();
    
private:
    
    CvCapture* _capture;
    cv::CascadeClassifier _faceCascade;
};

Q_DECLARE_METATYPE(cv::Mat)

#endif /* defined(__interface__Webcam__) */
