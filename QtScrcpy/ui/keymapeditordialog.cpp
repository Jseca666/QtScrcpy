#include "keymapeditordialog.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsObject>
#include <QPainter>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>

// -------------------- 可拖拽节点 Item --------------------
class ClickItem : public QGraphicsObject {
public:
  ClickItem(const QString& key, const QPointF& pos, QGraphicsItem* parent=nullptr)
    : QGraphicsObject(parent), m_key(key) {
    setPos(pos);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
  }

  QRectF boundingRect() const override { return QRectF(-10, -10, 20, 20); }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(isSelected() ? Qt::yellow : Qt::white);
    p->drawEllipse(boundingRect());
    // 小字显示 key（太长就截断）
    QString t = m_key;
    if (t.size() > 6) t = t.left(6) + "...";
    p->drawText(QRectF(-40, 12, 80, 20), Qt::AlignCenter, t);
  }

  QString key() const { return m_key; }
  void setKey(const QString& k) { m_key = k; update(); }

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override {
    Q_UNUSED(e);
    bool ok=false;
    QString k = QInputDialog::getText(nullptr, "Set Key", "Key (e.g. Key_F1 / LeftButton):",
                                      QLineEdit::Normal, m_key, &ok);
    if (ok && !k.trimmed().isEmpty()) setKey(k.trimmed());
  }

private:
  QString m_key;
};

// 多次点击：一个主点 + 若干子点（子点用小圆表示），并保存 clickNodes(delay,pos)
class MultiClickItem : public QGraphicsObject {
public:
  struct SubClick {
    int delayMs = 0;
    QPointF offset; // 相对主点的偏移（像素）
  };

  MultiClickItem(const QString& key, const QPointF& pos, QGraphicsItem* parent=nullptr)
    : QGraphicsObject(parent), m_key(key) {
    setPos(pos);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
  }

  QRectF boundingRect() const override { return QRectF(-40, -40, 80, 80); }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(isSelected() ? Qt::yellow : Qt::white);
    p->drawRect(QRectF(-12, -12, 24, 24)); // 主点用方框区分
    QString t = m_key;
    if (t.size() > 6) t = t.left(6) + "...";
    p->drawText(QRectF(-50, 14, 100, 20), Qt::AlignCenter, "MULTI:" + t);

    // 子点：小圆
    p->setPen(Qt::cyan);
    for (const auto& sc : m_sub) {
      QRectF r(-6 + sc.offset.x(), -6 + sc.offset.y(), 12, 12);
      p->drawEllipse(r);
      p->drawText(QRectF(-40 + sc.offset.x(), 8 + sc.offset.y(), 80, 16),
                  Qt::AlignCenter, QString::number(sc.delayMs));
    }
  }

  QString key() const { return m_key; }
  void setKey(const QString& k) { m_key = k; update(); }

  const std::vector<SubClick>& subClicks() const { return m_sub; }

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override {
    Q_UNUSED(e);
    // 双击：编辑 key，顺便添加一个子点（可选）
    bool ok=false;
    QString k = QInputDialog::getText(nullptr, "Set Key", "Key (e.g. Key_G):",
                                      QLineEdit::Normal, m_key, &ok);
    if (ok && !k.trimmed().isEmpty()) setKey(k.trimmed());

    // 再问要不要加子点
    int delay = QInputDialog::getInt(nullptr, "Add Sub Click", "Delay(ms) for new sub click:",
                                     40, 0, 5000, 10, &ok);
    if (!ok) return;
    // 默认在右上偏一点（你可以后续做“点击背景取点”增强）
    SubClick sc;
    sc.delayMs = delay;
    sc.offset = QPointF(25, -25);
    m_sub.push_back(sc);
    update();
  }

private:
  QString m_key;
  std::vector<SubClick> m_sub;
};

// -------------------- 对话框实现 --------------------
KeymapEditorDialog::KeymapEditorDialog(const QImage& background,
                                       const QString& keymapDir,
                                       QWidget* parent)
  : QDialog(parent), m_bg(background), m_keymapDir(keymapDir) {
  buildUi();
  rebuildBackground();
}

void KeymapEditorDialog::buildUi() {
  setWindowTitle("Keymap Editor (MVP)");
  resize(1100, 700);

  auto* root = new QVBoxLayout(this);
  m_view = new QGraphicsView(this);
  m_scene = new QGraphicsScene(this);
  m_view->setScene(m_scene);
  m_view->setDragMode(QGraphicsView::RubberBandDrag);
  root->addWidget(m_view, 1);

  auto* btnRow = new QHBoxLayout();
  auto* btnSave = new QPushButton("Save to keymap/", this);
  auto* btnClose = new QPushButton("Close", this);
  btnRow->addStretch(1);
  btnRow->addWidget(btnSave);
  btnRow->addWidget(btnClose);
  root->addLayout(btnRow);

  connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
  connect(btnSave, &QPushButton::clicked, this, &KeymapEditorDialog::saveToJson);

  // 右键菜单：通过 viewport 捕获
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint& p){
    QPointF scenePos = m_view->mapToScene(p);
    showContextMenu(m_view->viewport()->mapToGlobal(p), scenePos);
  });
}

void KeymapEditorDialog::rebuildBackground() {
  m_scene->clear();
  auto pix = QPixmap::fromImage(m_bg);
  auto* bgItem = new QGraphicsPixmapItem(pix);
  bgItem->setZValue(-1000);
  m_scene->addItem(bgItem);
  m_scene->setSceneRect(pix.rect());
}

void KeymapEditorDialog::showContextMenu(const QPoint& globalPos, const QPointF& scenePos) {
  QMenu menu;
  auto* a1 = menu.addAction("Add: Single Click (KMT_CLICK)");
  auto* a2 = menu.addAction("Add: Multi Click (KMT_CLICK_MULTI)");

  QAction* chosen = menu.exec(globalPos);
  if (!chosen) return;

  if (chosen == a1) addClickNode(scenePos);
  if (chosen == a2) addMultiClickNode(scenePos);
}

void KeymapEditorDialog::addClickNode(const QPointF& scenePos) {
  bool ok=false;
  QString k = QInputDialog::getText(this, "Key", "Key (e.g. Key_F1 / LeftButton):",
                                    QLineEdit::Normal, "Key_F1", &ok);
  if (!ok || k.trimmed().isEmpty()) return;

  auto* item = new ClickItem(k.trimmed(), scenePos);
  m_scene->addItem(item);
}

void KeymapEditorDialog::addMultiClickNode(const QPointF& scenePos) {
  bool ok=false;
  QString k = QInputDialog::getText(this, "Key", "Key (e.g. Key_G):",
                                    QLineEdit::Normal, "Key_G", &ok);
  if (!ok || k.trimmed().isEmpty()) return;

  auto* item = new MultiClickItem(k.trimmed(), scenePos);
  m_scene->addItem(item);
}

QPair<double,double> KeymapEditorDialog::toNorm(const QPointF& scenePos) const {
  double w = m_bg.width();
  double h = m_bg.height();
  double x = w > 0 ? scenePos.x() / w : 0.0;
  double y = h > 0 ? scenePos.y() / h : 0.0;
  // clamp
  if (x < 0) x = 0; if (x > 1) x = 1;
  if (y < 0) y = 0; if (y > 1) y = 1;
  return {x,y};
}

QJsonObject KeymapEditorDialog::buildKeymapJson() const {
  QJsonObject root;
  root["switchKey"] = "Key_QuoteLeft"; // MVP 固定；后面你可以做 UI 设置

  QJsonArray nodes;

  const auto items = m_scene->items();
  for (QGraphicsItem* gi : items) {
    if (auto* ci = dynamic_cast<ClickItem*>(gi)) {
      auto n = toNorm(ci->pos());
      QJsonObject obj;
      obj["type"] = "KMT_CLICK";
      obj["key"]  = ci->key();
      QJsonObject pos;
      pos["x"] = n.first;
      pos["y"] = n.second;
      obj["pos"] = pos;
      nodes.append(obj);
      continue;
    }

    if (auto* mi = dynamic_cast<MultiClickItem*>(gi)) {
      auto base = toNorm(mi->pos());
      QJsonObject obj;
      obj["type"] = "KMT_CLICK_MULTI";
      obj["key"]  = mi->key();

      QJsonArray clickNodes;
      // 主点也算一次点击（delay=0）
      {
        QJsonObject cn;
        cn["delay"] = 0;
        QJsonObject p;
        p["x"] = base.first;
        p["y"] = base.second;
        cn["pos"] = p;
        clickNodes.append(cn);
      }
      // 子点
      for (const auto& sc : mi->subClicks()) {
        QPointF ppx = mi->pos() + sc.offset;
        auto nn = toNorm(ppx);
        QJsonObject cn;
        cn["delay"] = sc.delayMs;
        QJsonObject p;
        p["x"] = nn.first;
        p["y"] = nn.second;
        cn["pos"] = p;
        clickNodes.append(cn);
      }
      obj["clickNodes"] = clickNodes;
      nodes.append(obj);
      continue;
    }
  }

  root["keyMapNodes"] = nodes;
  return root;
}

void KeymapEditorDialog::saveToJson() {
  bool ok=false;
  QString name = QInputDialog::getText(this, "Save As", "Script file name (english, no .json):",
                                       QLineEdit::Normal, "my_map", &ok);
  if (!ok || name.trimmed().isEmpty()) return;

  QString base = name.trimmed();
  // 简单过滤
  base.replace(" ", "_");

  QDir dir(m_keymapDir);
  if (!dir.exists()) dir.mkpath(".");

  QString fullPath = dir.filePath(base + ".json");

  QJsonObject root = buildKeymapJson();
  QJsonDocument doc(root);

  QFile f(fullPath);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::critical(this, "Error", "Cannot write: " + fullPath);
    return;
  }
  f.write(doc.toJson(QJsonDocument::Indented));
  f.close();

  m_last.filename = base + ".json";
  m_last.fullPath = fullPath;

  emit saved(m_last.filename, m_last.fullPath);

  QMessageBox::information(this, "Saved", "Saved:\n" + fullPath);
}