// Stand-in for the filter of the volume module: it drops the dummy output.
import org.kde.kitemmodels as KItemModels

KItemModels.KSortFilterProxyModel {
    property bool filterOutInactiveDevices: false
    property bool filterVirtualDevices: false
    filterRowCallback: (row, parent) => sourceModel.data(sourceModel.index(row, 0, parent),
                                                         sourceModel.KItemModels.KRoleNames.role("Name")) !== "auto_null"
}
