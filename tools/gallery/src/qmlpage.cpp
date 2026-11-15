// A Qt Quick page: one QML file of the program's resources in a
// QQuickWidget, drawn by the Qt Quick style set in main (org.kde.desktop,
// which paints with the same widget style as the Qt Widgets pages).

#include "page.h"

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QVBoxLayout>

namespace
{
class QmlPage : public GalleryPage
{
public:
    QmlPage(const QString &file, const PageContext &context)
    {
        setObjectName(file);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_view = new QQuickWidget;
        m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
        m_view->setFocusPolicy(Qt::StrongFocus);
        QQmlEngine *engine = m_view->engine();
        for (const QString &dir : context.qmlImportDirs) {
            engine->addImportPath(dir);
        }
        connect(engine, &QQmlEngine::warnings, this, [this](const QList<QQmlError> &warnings) {
            for (const QQmlError &w : warnings) {
                m_warnings << w.toString();
            }
        });
        m_view->setSource(QUrl(QStringLiteral("qrc:/gallery/qml/") + file));
        layout->addWidget(m_view);
    }

    // by object name, through the visual tree: the delegates of a view
    // have the view as visual parent only
    static QQuickItem *findItem(QQuickItem *from, const QString &name)
    {
        if (!from) {
            return nullptr;
        }
        if (from->objectName() == name) {
            return from;
        }
        const auto children = from->childItems();
        for (QQuickItem *child : children) {
            if (QQuickItem *found = findItem(child, name)) {
                return found;
            }
        }
        return nullptr;
    }

    QQuickItem *item(const char *name) const
    {
        QQuickItem *root = m_view->rootObject();
        if (!root) {
            return nullptr;
        }
        if (QQuickItem *found = findItem(root, QLatin1String(name))) {
            return found;
        }
        // popups (menus, dialogs) are not in the page's visual tree
        return root->findChild<QQuickItem *>(QLatin1String(name));
    }

    QRect hoverRect() const override
    {
        QQuickItem *target = item("hoverTarget");
        if (!target || !target->isVisible() || !target->window()) {
            return {};
        }
        const QRectF r = target->mapRectToScene(QRectF(0, 0, target->width(), target->height()));
        return QRect(m_view->mapToGlobal(r.topLeft().toPoint()), r.size().toSize());
    }

    void focusSample() override
    {
        QQuickItem *target = item("focusTarget");
        if (!target) {
            return;
        }
        m_view->setFocus(Qt::TabFocusReason);
        target->forceActiveFocus(Qt::TabFocusReason);
    }

    void openPopups() override
    {
        call("openPopups()");
    }

    void closePopups() override
    {
        call("closePopups()");
    }

    QStringList problems() const override
    {
        QStringList list;
        const auto errors = m_view->errors();
        for (const QQmlError &e : errors) {
            list << e.toString();
        }
        if (!m_view->rootObject()) {
            list << QStringLiteral("%1 did not load").arg(objectName());
        }
        return list;
    }

    QStringList warnings() const override
    {
        return m_warnings;
    }

private:
    void call(const char *signature)
    {
        QQuickItem *root = m_view->rootObject();
        if (!root || root->metaObject()->indexOfMethod(signature) < 0) {
            return;
        }
        const QByteArray name = QByteArray(signature).chopped(2);
        QMetaObject::invokeMethod(root, name.constData());
    }

    QQuickWidget *m_view = nullptr;
    QStringList m_warnings;
};
}

GalleryPage *makeQmlPage(const QString &qmlFile, const PageContext &context)
{
    return new QmlPage(qmlFile, context);
}
