#pragma once
#include <QDialog>
#include <QImage>
#include <QJsonObject>

class QGraphicsView;
class QGraphicsScene;

class KeymapEditorDialog : public QDialog {
  Q_OBJECT
public:
  struct SaveResult {
    QString filename;   // 保存的 json 文件名（不含路径）
    QString fullPath;   // 完整路径
  };

  KeymapEditorDialog(const QImage& background,
                     const QString& keymapDir,
                     QWidget* parent = nullptr);

  SaveResult lastSaveResult() const { return m_last; }

signals:
  void saved(const QString& filename, const QString& fullPath);

private:
  void buildUi();
  void rebuildBackground();
  void showContextMenu(const QPoint& globalPos, const QPointF& scenePos);

  void addClickNode(const QPointF& scenePos);
  void addMultiClickNode(const QPointF& scenePos);

  void saveToJson();

  // 将 scene 像素坐标转相对坐标(0~1)
  QPair<double,double> toNorm(const QPointF& scenePos) const;

  // 生成 QtScrcpy 兼容 json
  QJsonObject buildKeymapJson() const;

private:
  QImage m_bg;
  QString m_keymapDir;

  QGraphicsView* m_view = nullptr;
  QGraphicsScene* m_scene = nullptr;

  SaveResult m_last;
};