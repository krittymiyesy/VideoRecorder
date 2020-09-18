
#ifndef PUSHPOINT_H
#define PUSHPOINT_H

#include <QLabel>
#include <QMouseEvent>


#define POINT_SIZE 7  //�ı䷶Χ�İ˸���Ĵ�С


/***
 ***�ı䷶Χʱ����ʾ�ı߿��еİ˸����
 ***���е�һ�������
 ***/
class PushPoint : public QLabel
{
    Q_OBJECT
    
public:
    explicit PushPoint(QWidget *parent = 0);
    ~PushPoint();
    enum LocationPoint //�˵����ڵ�λ��
    {
        TopLeft = 0, //����
        TopRight= 1, //����
        TopMid  = 2, //����
        BottomLeft= 3, //����
        BottomRight= 4, //����
        BottomMid= 5, //����
        MidLeft= 6, //����
        MidRight= 7 //����
    };
    void setLocation(LocationPoint);
    LocationPoint locPoint(){return location;}
protected:
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);

signals:
    void moved(QPoint);
private:
    QPoint dragPosition;

    LocationPoint location; //�˵��λ��

    void setMouseCursor(); //���������״
};

#endif // PUSHPOINT_H
