/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// Description : interface for the CViewport class.


#pragma once

#if !defined(Q_MOC_RUN)
#include <AzToolsFramework/Viewport/ViewportTypes.h>
#include <AzToolsFramework/ViewportUi/ViewportUiManager.h>
#include <Cry_Color.h>
#include "IPostRenderer.h"
#include "Include/IDisplayViewport.h"
#include "Include/SandboxAPI.h"

#include <QMenu>
#include <QPointer>

#if defined(Q_OS_WIN)
#include <QtWinExtras/qwinfunctions.h>
#endif

#include <AzCore/Math/Uuid.h>
#include <IEditor.h>
#endif

namespace AzQtComponents
{
    class ViewportDragContext;
}

// forward declarations.
class CBaseObject;
struct DisplayContext;
class CCryEditDoc;
class CLayoutViewPane;
class CViewManager;
class CBaseObjectsCache;
struct HitContext;
struct IRenderListener;
class CImageEx;
class QMenu;
struct IDataBaseItem;

/** Type of viewport.
*/
enum EViewportType
{
    ET_ViewportUnknown = 0,
    ET_ViewportXY,
    ET_ViewportXZ,
    ET_ViewportYZ,
    ET_ViewportCamera,
    ET_ViewportMap,
    ET_ViewportModel,
    ET_ViewportZ, //!< Z Only viewport.
    ET_ViewportUI,

    ET_ViewportLast,
};

//////////////////////////////////////////////////////////////////////////
// Standart cursors viewport can display.
//////////////////////////////////////////////////////////////////////////
enum EStdCursor
{
    STD_CURSOR_DEFAULT,
    STD_CURSOR_HIT,
    STD_CURSOR_MOVE,
    STD_CURSOR_ROTATE,
    STD_CURSOR_SCALE,
    STD_CURSOR_SEL_PLUS,
    STD_CURSOR_SEL_MINUS,
    STD_CURSOR_SUBOBJ_SEL,
    STD_CURSOR_SUBOBJ_SEL_PLUS,
    STD_CURSOR_SUBOBJ_SEL_MINUS,
    STD_CURSOR_HAND,

    STD_CURSOR_GAME,

    STD_CURSOR_LAST,
};

AZ_PUSH_DISABLE_DLL_EXPORT_BASECLASS_WARNING
class SANDBOX_API CViewport
    : public IDisplayViewport
{
AZ_POP_DISABLE_DLL_EXPORT_BASECLASS_WARNING
public:
    typedef void(* DropCallback)(CViewport* viewport, int ptx, int pty, void* custom);

    virtual ~CViewport() {}

    virtual void SetActiveWindow() = 0;

    // Changed my view manager.
    void SetViewManager(CViewManager* viewMgr) { m_viewManager = viewMgr; };

    //! Access to view manager.
    CViewManager* GetViewManager() const { return m_viewManager; };

    virtual void RegisterRenderListener(IRenderListener*    piListener) = 0;
    virtual bool UnregisterRenderListener(IRenderListener*  piListener) = 0;
    virtual bool IsRenderListenerRegistered(IRenderListener*    piListener) = 0;

    virtual void AddPostRenderer(IPostRenderer* pPostRenderer) = 0;
    virtual bool RemovePostRenderer(IPostRenderer* pPostRenderer) = 0;

    virtual bool DestroyWindow() { return false; }

    /** Get type of this viewport.
    */
    virtual EViewportType GetType() const { return ET_ViewportUnknown; }
    /** Must be overriden in derived classes.
    */
    virtual void SetType(EViewportType type) = 0;

    /** Get name of viewport
    */
    virtual QString GetName() const = 0;

    virtual void SetSelected([[maybe_unused]] bool const bSelect){}

    //! Resets current selection region.
    virtual void ResetSelectionRegion() = 0;
    //! Set 2D selection rectangle.
    virtual void SetSelectionRectangle(const QRect& rect) = 0;
    inline void SetSelectionRectangle(const QPoint& startMousePosition, const QPoint& currentMousePosition)
    {
        // QRect's bottom/right are width - 1, height - 1, so when specifying the right position
        // directly in a QRect, we need to offset it by -1.
        SetSelectionRectangle(QRect(startMousePosition, currentMousePosition - QPoint(1, 1)));
    }
    //! Return 2D selection rectangle.
    virtual QRect GetSelectionRectangle() const = 0;
    //! Called when dragging selection rectangle.
    virtual void OnDragSelectRectangle(const QRect& rect, bool bNormalizeRect = false) = 0;
    void OnDragSelectRectangle(const QPoint& startMousePosition, const QPoint& currentMousePosition, bool bNormalizeRect = false)
    {
        // QRect's bottom/right are width - 1, height - 1, so when specifying the right position
        // directly in a QRect, we need to offset it by -1.
        OnDragSelectRectangle(QRect(startMousePosition, currentMousePosition - QPoint(1, 1)), bNormalizeRect);
    }

    virtual void ResetContent() = 0;
    virtual void UpdateContent(int flags) = 0;

    virtual void SetAxisConstrain(int axis) = 0;
    int GetAxisConstrain() const { return GetIEditor()->GetAxisConstrains(); };

    virtual Vec3 SnapToGrid(const Vec3& vec) = 0;

    //! Get selection procision tolerance.
    virtual float GetSelectionTolerance() const = 0;

    //////////////////////////////////////////////////////////////////////////
    // View matrix.
    //////////////////////////////////////////////////////////////////////////
    //! Set current view matrix,
    //! This is a matrix that transforms from world to view space.
    virtual void SetViewTM([[maybe_unused]] const Matrix34& tm)
    {
        AZ_Error("CryLegacy", false, "QtViewport::SetViewTM not implemented");
    }

    //! Get current view matrix.
    //! This is a matrix that transforms from world space to view space.
    virtual const Matrix34& GetViewTM() const
    {
        AZ_Error("CryLegacy", false, "QtViewport::GetViewTM not implemented");
        static const Matrix34 m;
        return m;
    };

    //////////////////////////////////////////////////////////////////////////
    //! Get current screen matrix.
    //! Screen matrix transform from World space to Screen space.
    virtual const Matrix34& GetScreenTM() const
    {
        return m_screenTM;
    }

    virtual Vec3 MapViewToCP(const QPoint& point) = 0;

    //! Map viewport position to world space position.
    virtual Vec3        ViewToWorld(const QPoint& vp, bool* pCollideWithTerrain = nullptr, bool onlyTerrain = false, bool bSkipVegetation = false, bool bTestRenderMesh = false, bool* collideWithObject = nullptr) const = 0;
    //! Convert point on screen to world ray.
    virtual void        ViewToWorldRay(const QPoint& vp, Vec3& raySrc, Vec3& rayDir) const = 0;
    //! Get normal for viewport position
    virtual Vec3        ViewToWorldNormal(const QPoint& vp, bool onlyTerrain, bool bTestRenderMesh = false) = 0;

    virtual Vec3 GetCPVector(const Vec3& p1, const Vec3& p2, int axis) = 0;
    virtual Vec3 GetCPVector(const Vec3& p1, const Vec3& p2) { return GetCPVector(p1, p2, GetAxisConstrain()); }

    //! Performs hit testing of 2d point in view to find which object hit.
    virtual bool HitTest(const QPoint& point, HitContext& hitInfo) = 0;
    virtual AZ::Vector3 GetHitLocation(const QPoint& point) = 0;

    virtual void MakeConstructionPlane(int axis) = 0;

    // Access to the member m_bAdvancedSelectMode so interested modules can know its value.
    virtual bool GetAdvancedSelectModeFlag() = 0;

    virtual void ToggleCameraObject() {}
    virtual bool IsSequenceCamera() const { return false;  }

    //! Center viewport on selection.
    virtual void CenterOnSelection() = 0;
    virtual void CenterOnAABB(const AABB& aabb) = 0;

    virtual void CenterOnSliceInstance() = 0;

    /** Set ID of this viewport
    */
    virtual void SetViewportId(int id) { m_nCurViewportID = id; };

    /** Get ID of this viewport
    */
    virtual int GetViewportId() const { return m_nCurViewportID; };

    // Store final Game Matrix ready for editor
    void SetGameTM(const Matrix34& tm) { m_gameTM = tm; };

    //////////////////////////////////////////////////////////////////////////
    // Drag and drop support on viewports.
    // To be overrided in derived classes.
    //////////////////////////////////////////////////////////////////////////
    virtual bool CanDrop([[maybe_unused]] const QPoint& point, [[maybe_unused]] IDataBaseItem* pItem) { return false; };
    virtual void Drop([[maybe_unused]] const QPoint& point, [[maybe_unused]] IDataBaseItem* pItem) {};
    virtual void SetGlobalDropCallback(DropCallback dropCallback, void* dropCallbackCustom)
    {
        m_dropCallback = dropCallback;
        m_dropCallbackCustom = dropCallbackCustom;
    }

    virtual void SetConstructionMatrix(RefCoordSys coordSys, const Matrix34& xform) = 0;

    virtual void BeginUndo() = 0;
    virtual void AcceptUndo(const QString& undoDescription) = 0;
    virtual void CancelUndo() = 0;
    virtual void RestoreUndo() = 0;
    virtual bool IsUndoRecording() const = 0;

    virtual void CaptureMouse() {};
    virtual void SetCapture() { CaptureMouse(); }
    virtual void ReleaseMouse() {};

    virtual void ResetCursor() = 0;
    virtual void SetCursor(const QCursor& cursor) = 0;

    virtual void SetCurrentCursor(EStdCursor stdCursor) = 0;
    virtual void SetCurrentCursor(EStdCursor stdCursor, const QString& str) = 0;
    virtual void SetSupplementaryCursorStr(const QString& str) = 0;
    virtual void SetCursorString(const QString& str) = 0;

    virtual void SetFocus() = 0;
    virtual void Invalidate(bool bErase = 1) = 0;

    // Is overridden by RenderViewport
    virtual void SetFOV([[maybe_unused]] float fov) {}
    virtual float GetFOV() const;

    virtual QObject* qobject() { return nullptr; }
    virtual QWidget* widget() { return nullptr; }

    virtual void OnTitleMenu([[maybe_unused]] QMenu* menu) {}

    void SetViewPane(CLayoutViewPane* viewPane) { m_viewPane = viewPane; }

    //Child classes can override these to provide extra logic that wraps
    //widget rendering. Needed by the RenderViewport to handle raycasts
    //from screen-space to world-space.
    virtual void PreWidgetRendering() {}
    virtual void PostWidgetRendering() {}

    virtual CViewport *asCViewport() { return this; }

protected:
    CLayoutViewPane* m_viewPane = nullptr;
    CViewManager* m_viewManager;
    AZ_PUSH_DISABLE_DLL_EXPORT_MEMBER_WARNING
    // Screen Matrix
    Matrix34 m_screenTM;
    int m_nCurViewportID;
    // Final game view matrix before drpping back to editor
    Matrix34 m_gameTM;
    AZ_POP_DISABLE_DLL_EXPORT_MEMBER_WARNING

    // Custom drop callback (Leroy@Conffx)
    DropCallback m_dropCallback;
    void* m_dropCallbackCustom;
};

template<typename T>
typename std::enable_if<std::is_base_of<QObject, typename std::remove_cv<typename std::remove_pointer<T>::type>::type>::value, T>::type
viewport_cast(CViewport* viewport)
{
    if (viewport == nullptr)
    {
        return nullptr;
    }
    if (QObject* obj = viewport->qobject())
    {
        return qobject_cast<T>(obj);
    }
    else
    {
        return nullptr;
    }
}

/** Base class for all Editor Viewports.
*/
class SANDBOX_API QtViewport
    : public QWidget
    , public CViewport
{
    Q_OBJECT
public:
    QObject* qobject() override { return this; }
    QWidget* widget() override { return this; }

    template<typename T>
    static const GUID& GetClassID()
    {
        static GUID guid = [] {
            return AZ::Uuid::CreateRandom();
        } ();
        return guid;
    }

    QtViewport(QWidget* parent = nullptr);
    virtual ~QtViewport();

    void SetActiveWindow() override { activateWindow(); }

    //! Called while window is idle.
    virtual void Update();

    /** Set name of this viewport.
    */
    void SetName(const QString& name);

    /** Get name of viewport
    */
    QString GetName() const;

    virtual void SetFocus() { setFocus(); }
    virtual void Invalidate([[maybe_unused]] bool bErase = 1) { update(); }

    // Is overridden by RenderViewport
    virtual void SetFOV([[maybe_unused]] float fov) {}
    virtual float GetFOV() const;

    // Must be overridden in derived classes.
    // Returns:
    //   e.g. 4.0/3.0
    virtual float GetAspectRatio() const = 0;
    virtual void GetDimensions(int* pWidth, int* pHeight) const;
    virtual void ScreenToClient(QPoint& pPoint) const override;

    virtual void ResetContent();
    virtual void UpdateContent(int flags);

    //! Set current zoom factor for this viewport.
    virtual void SetZoomFactor(float fZoomFactor);

    //! Get current zoom factor for this viewport.
    virtual float GetZoomFactor() const;

    virtual void OnActivate();
    virtual void OnDeactivate();

    //! Map world space position to viewport position.
    virtual QPoint   WorldToView(const Vec3& wp) const override;

    //! Map world space position to 3D viewport position.
    virtual Vec3    WorldToView3D(const Vec3& wp, int nFlags = 0) const;

    //! Map viewport position to world space position.
    virtual Vec3 ViewToWorld(const QPoint& vp, bool* pCollideWithTerrain = nullptr, bool onlyTerrain = false, bool bSkipVegetation = false, bool bTestRenderMesh = false, bool* collideWithObject = nullptr) const override;
    //! Convert point on screen to world ray.
    virtual void        ViewToWorldRay(const QPoint& vp, Vec3& raySrc, Vec3& rayDir) const override;
    //! Get normal for viewport position
    virtual Vec3        ViewToWorldNormal(const QPoint& vp, bool onlyTerrain, bool bTestRenderMesh = false) override;

    //! Map view point to world space using current construction plane.
    Vec3 MapViewToCP(const QPoint& point) override { return MapViewToCP(point, GetAxisConstrain()); }
    virtual Vec3 MapViewToCP(const QPoint& point, int axis);

    //! This method return a vector (p2-p1) in world space alligned to construction plane and restriction axises.
    //! p1 and p2 must be given in world space and lie on construction plane.
    virtual Vec3 GetCPVector(const Vec3& p1, const Vec3& p2, int axis);

    //! Snap any given 3D world position to grid lines if snap is enabled.
    Vec3 SnapToGrid(const Vec3& vec) override;
    virtual float GetGridStep() const;

    //! Returns the screen scale factor for a point given in world coordinates.
    //! This factor gives the width in world-space units at the point's distance of the viewport.
    virtual float GetScreenScaleFactor([[maybe_unused]] const Vec3& worldPoint) const { return 1; };

    void SetAxisConstrain(int axis);

    /// Take raw input and create a final mouse interaction.
    /// @attention Do not map **point** from widget to viewport explicitly,
    /// this is handled internally by BuildMouseInteraction - just pass directly.
    virtual AzToolsFramework::ViewportInteraction::MouseInteraction BuildMouseInteraction(
        Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, const QPoint& point);

    //////////////////////////////////////////////////////////////////////////
    // Selection.
    //////////////////////////////////////////////////////////////////////////
    //! Resets current selection region.
    virtual void ResetSelectionRegion();
    //! Set 2D selection rectangle.
    void SetSelectionRectangle(const QRect& rect) override;

    //! Return 2D selection rectangle.
    QRect GetSelectionRectangle() const override { return m_selectedRect; };
    //! Called when dragging selection rectangle.
    void OnDragSelectRectangle(const QRect& rect, bool bNormalizeRect = false) override;
    //! Get selection procision tolerance.
    float GetSelectionTolerance() const { return m_selectionTolerance; }
    //! Center viewport on selection.
    void CenterOnSelection() override {}
    void CenterOnAABB([[maybe_unused]] const AABB& aabb) override {}

    virtual void CenterOnSliceInstance() {}

    //! Performs hit testing of 2d point in view to find which object hit.
    bool HitTest(const QPoint& point, HitContext& hitInfo) override;
    AZ::Vector3 GetHitLocation(const QPoint& point) override;

    //! Do 2D hit testing of line in world space.
    // pToCameraDistance is an optional output parameter in which distance from the camera to the line is returned.
    bool HitTestLine(const Vec3& lineP1, const Vec3& lineP2, const QPoint& hitpoint, int pixelRadius, float* pToCameraDistance = 0) const override;

    float GetDistanceToLine(const Vec3& lineP1, const Vec3& lineP2, const QPoint& point) const override;

    // Access to the member m_bAdvancedSelectMode so interested modules can know its value.
    bool GetAdvancedSelectModeFlag();

    virtual void GetPerpendicularAxis(EAxis* pAxis, bool* pIs2D) const;
    virtual const ::Plane* GetConstructionPlane() const { return &m_constructionPlane; }

    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    //! Set construction plane from given position construction matrix refrence coord system and axis settings.
    //////////////////////////////////////////////////////////////////////////
    void MakeConstructionPlane(int axis) override;
    virtual void SetConstructionMatrix(RefCoordSys coordSys, const Matrix34& xform);
    virtual const Matrix34& GetConstructionMatrix(RefCoordSys coordSys);
    // Set simple construction plane origin.
    void SetConstructionOrigin(const Vec3& worldPos);
    //////////////////////////////////////////////////////////////////////////

    void DegradateQuality(bool bEnable);

    //////////////////////////////////////////////////////////////////////////
    // Undo for viewpot operations.
    void BeginUndo();
    void AcceptUndo(const QString& undoDescription);
    void CancelUndo();
    void RestoreUndo();
    bool IsUndoRecording() const;
    //////////////////////////////////////////////////////////////////////////

    //! Get prefered original size for this viewport.
    //! if 0, then no preference.
    virtual QSize GetIdealSize() const;

    //! Check if world space bounding box is visible in this view.
    virtual bool IsBoundsVisible(const AABB& box) const;

    //////////////////////////////////////////////////////////////////////////

    void SetCursor(const QCursor& cursor)
    {
        setCursor(cursor);
    }

    // Set`s current cursor string.
    void SetCurrentCursor(const QCursor& hCursor, const QString& cursorString);
    virtual void SetCurrentCursor(EStdCursor stdCursor, const QString& cursorString);
    void SetCurrentCursor(EStdCursor stdCursor);
    virtual void SetCursorString(const QString& cursorString);
    void ResetCursor();
    void SetSupplementaryCursorStr(const QString& str);

    //////////////////////////////////////////////////////////////////////////
    // Return visble objects cache.
    CBaseObjectsCache* GetVisibleObjectsCache() { return m_pVisibleObjectsCache; };

    void RegisterRenderListener(IRenderListener*    piListener);
    bool UnregisterRenderListener(IRenderListener*  piListener);
    bool IsRenderListenerRegistered(IRenderListener*    piListener);

    void AddPostRenderer(IPostRenderer* pPostRenderer);
    bool RemovePostRenderer(IPostRenderer* pPostRenderer);

    void CaptureMouse() override { m_mouseCaptured = true;  QWidget::grabMouse(); }
    void ReleaseMouse() override { m_mouseCaptured = false;  QWidget::releaseMouse(); }

    virtual void setRay(QPoint& vp, Vec3& raySrc, Vec3& rayDir);
    virtual void setHitcontext(QPoint& vp, Vec3& raySrc, Vec3& rayDir);
    QPoint m_vp;
    AZ_PUSH_DISABLE_DLL_EXPORT_MEMBER_WARNING
    Vec3 m_raySrc;
    Vec3 m_rayDir;

    // Greater than 0 while running MouseCallback() function. It needs to be a counter
    // because of recursive calls to MouseCallback(). It's used to make an exception
    // during the SScopedCurrentContext count check of m_cameraSetForWidgetRenderingCount.
    int m_processingMouseCallbacksCounter = 0;

    AZ_POP_DISABLE_DLL_EXPORT_MEMBER_WARNING
protected:
    friend class CViewManager;
    bool IsVectorInValidRange(const Vec3& v) const { return fabs(v.x) < 1e+8 && fabs(v.y) < 1e+8 && fabs(v.z) < 1e+8; }
    void AssignConstructionPlane(const Vec3& p1, const Vec3& p2, const Vec3& p3);
    HWND renderOverlayHWND() const;
    void setRenderOverlayVisible(bool);
    bool isRenderOverlayVisible() const;

    // called to process mouse callback inside the viewport.
    virtual bool MouseCallback(EMouseEvent event, const QPoint& point, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons = Qt::NoButton);

    void ProcessRenderLisneters(DisplayContext& rstDisplayContext);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

    void paintEvent(QPaintEvent* event) override;

    virtual void OnMouseMove(Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons, const QPoint& point);
    virtual void OnMouseWheel(Qt::KeyboardModifiers modifiers, short zDelta, const QPoint& pt);
    virtual void OnLButtonDown(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnLButtonUp(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnRButtonDown(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnRButtonUp(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnMButtonDblClk(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnMButtonDown(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnMButtonUp(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnLButtonDblClk(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnRButtonDblClk(Qt::KeyboardModifiers modifiers, const QPoint& point);
    virtual void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    virtual void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
#if defined(AZ_PLATFORM_WINDOWS)
    void OnRawInput(UINT wParam, HRAWINPUT lParam);
#endif
    void OnSetCursor();

    virtual void BuildDragDropContext(AzQtComponents::ViewportDragContext& context, const QPoint& pt);
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    //Child classes can override these to provide extra logic that wraps
    //widget rendering. Needed by the RenderViewport to handle raycasts
    //from screen-space to world-space.
    virtual void PreWidgetRendering() {}
    virtual void PostWidgetRendering() {}

    AZ_PUSH_DISABLE_DLL_EXPORT_MEMBER_WARNING
    AzToolsFramework::ViewportUi::ViewportUiManager m_viewportUi;
    AZ_POP_DISABLE_DLL_EXPORT_MEMBER_WARNING

    float m_selectionTolerance;
    QMenu m_cViewMenu;

    mutable float m_fZoomFactor;

    QPoint m_cMouseDownPos;

    //! Current selected rectangle.
    QRect m_selectedRect;

    int m_activeAxis;

    AZ_PUSH_DISABLE_DLL_EXPORT_MEMBER_WARNING
    //! Current construction plane.
    ::Plane m_constructionPlane;
    Vec3 m_constructionPlaneAxisX;
    Vec3 m_constructionPlaneAxisY;
    AZ_POP_DISABLE_DLL_EXPORT_MEMBER_WARNING

    // When true selection helpers will be shown and hit tested against.
    bool m_bAdvancedSelectMode;

    //////////////////////////////////////////////////////////////////////////
    // Standard cursors.
    //////////////////////////////////////////////////////////////////////////
    QCursor m_hCursor[STD_CURSOR_LAST];
    QCursor m_hCurrCursor;

    //! Mouse is over this object.
    CBaseObject* m_pMouseOverObject;
    QString m_cursorStr;
    QString m_cursorSupplementaryStr;

    static bool m_bDegradateQuality;

    // Grid size modifier due to zoom.
    float m_fGridZoom;

    int m_nLastUpdateFrame;
    int m_nLastMouseMoveFrame;

    CBaseObjectsCache* m_pVisibleObjectsCache;

    QRect m_rcClient;

    AZ_PUSH_DISABLE_DLL_EXPORT_MEMBER_WARNING
    // Same construction matrix is shared by all viewports.
    Matrix34 m_constructionMatrix[LAST_COORD_SYSTEM];

    std::vector<IRenderListener*>           m_cRenderListeners;

    typedef std::vector<_smart_ptr<IPostRenderer> > PostRenderers;
    PostRenderers   m_postRenderers;
    AZ_POP_DISABLE_DLL_EXPORT_MEMBER_WARNING

protected:
    bool m_mouseCaptured = false;

private:
    QWidget m_renderOverlay;
};
