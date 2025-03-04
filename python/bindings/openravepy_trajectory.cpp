// -*- coding: utf-8 -*-
// Copyright (C) 2006-2011 Rosen Diankov <rosen.diankov@gmail.com>
//
// This file is part of OpenRAVE.
// OpenRAVE is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#define NO_IMPORT_ARRAY
#include <openravepy/openravepy_int.h>
#include <openravepy/openravepy_configurationspecification.h>
#include <openravepy/openravepy_environmentbase.h>
#include <openravepy/openravepy_trajectorybase.h>

#ifndef USE_PYBIND11_PYTHON_BINDINGS
#include <boost/python/slice.hpp> // slice objects
#endif

namespace openravepy {

using py::object;
using py::extract;
using py::extract_;
using py::handle;
using py::dict;
using py::enum_;
using py::class_;
using py::init;
using py::scope_; // py::object if USE_PYBIND11_PYTHON_BINDINGS
using py::scope;
using py::args;
using py::return_value_policy;
using py::slice;

#ifndef USE_PYBIND11_PYTHON_BINDINGS
using py::no_init;
using py::bases;
using py::copy_const_reference;
using py::docstring_options;
using py::pickle_suite;
using py::manage_new_object;
using py::def;
#endif // USE_PYBIND11_PYTHON_BINDINGS

namespace numeric = py::numeric;

/// \brief converts python contigous array to stl vector
/// \param[IN] oContiguousArray input array to convert from
/// \param[OUT] vecOut output to where converted result is saved
/// \return whether oContiguousArray is succesfully converted to vecOut
template <typename T>
inline bool ExtractContiguousArrayToVector(py::object oContiguousArray,
                                           std::vector<T>& vecOut)
{
    vecOut.clear();
    if (IS_PYTHONOBJECT_NONE(oContiguousArray)) {
        return true;
    }

    if (!py::isinstance<py::array_t<T>>(oContiguousArray)) {
        RAVELOG_WARN("Input data is not contigous or element type is different from expected, so have to use slow generic conversion. Please use numpy array to take advantage of faster conversion making use of contiguous memory allocation of it.");
        return false;
    }

    PyArrayObject* arrPtr = PyArray_GETCONTIGUOUS((PyArrayObject*)oContiguousArray.ptr());
    AutoPyArrayObjectDereferencer psaver(arrPtr);
    if( !arrPtr || !arrPtr->data) {
        RAVELOG_WARN("Input data is not contigous so have to use slow conversion. Please use numpy array to take advantage of faster conversion making use of contiguous memory allocation of it.");
        return false;
    }

    int pointsnum = PyArray_SIZE(arrPtr);
    if (pointsnum > 0) {
        T* pdata = (T*) PyArray_DATA(arrPtr);
        vecOut.resize(pointsnum);
        memcpy(&vecOut[0], pdata, pointsnum * sizeof(T));
    }
    return true;
}


/// \brief converts python contigous array to raw pointer
/// \param[IN] oContiguousArray input array to convert from
/// \param[OUT] ppdata pointer to start address of input contiguous array. *ppdata could be nullptr if failed to convert input.
/// \return number of elements in the input contiguous array. if negative, failed to convert input array into raw pointer. if input is null (None), 0 is returned.
template <typename T>
inline int ExtractContiguousArrayToPointer(py::object oContiguousArray,
                                           T** ppdata)
{
    *ppdata = nullptr;
    if (IS_PYTHONOBJECT_NONE(oContiguousArray)) {
        return 0;
    }

    if (!py::isinstance<py::array_t<T>>(oContiguousArray)) {
        RAVELOG_WARN("Element type is different from expected, so have to use slow generic conversion. Please use numpy array to take advantage of faster conversion making use of contiguous memory allocation of it.");
        return -1;
    }

    PyArrayObject* arrPtr = PyArray_GETCONTIGUOUS((PyArrayObject*)oContiguousArray.ptr());
    AutoPyArrayObjectDereferencer psaver(arrPtr);
    if( !arrPtr || !arrPtr->data) {
        RAVELOG_WARN("Input data is not contiguous so have to use slow conversion. Please use numpy array to take advantage of faster conversion making use of contiguous memory allocation of it.");
        return -1;
    }

    if( Py_REFCNT(arrPtr) == 1 ) {
        // ExtractArray does not support multidimensional array, so throw exception.
        // return -1;
        throw OPENRAVE_EXCEPTION_FORMAT0(_("Input data is not contiguous and cannot convert the input. Please flatten the input to make memory contiguous."),ORE_InvalidArguments);
    }

    int pointsnum = PyArray_SIZE(arrPtr);
    *ppdata = (T*) PyArray_DATA(arrPtr);
    return pointsnum;
}

thread_local std::vector<dReal> PyTrajectoryBase::_vdataCache, PyTrajectoryBase::_vtimesCache;

PyTrajectoryBase::PyTrajectoryBase(TrajectoryBasePtr pTrajectory, PyEnvironmentBasePtr pyenv) : PyInterfaceBase(pTrajectory, pyenv),_ptrajectory(pTrajectory) {
}
PyTrajectoryBase::~PyTrajectoryBase() {
}

void PyTrajectoryBase::Init(PyConfigurationSpecificationPtr pyspec) {
    _ptrajectory->Init(openravepy::GetConfigurationSpecification(pyspec));
}

void PyTrajectoryBase::Init(OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) {
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    this->Init(pyspec);
}

void PyTrajectoryBase::Insert(size_t index, object odata)
{
    dReal* pdata = nullptr;
    const int numElements = ExtractContiguousArrayToPointer(odata, &pdata);
    if (numElements >= 0) {
        _ptrajectory->Insert(index, pdata, numElements);
    }
    else {
        std::vector<dReal>& vdata = _vdataCache;
        vdata = ExtractArray<dReal>(odata);
        _ptrajectory->Insert(index,vdata);
    }
}

void PyTrajectoryBase::Insert(size_t index, object odata, bool bOverwrite)
{
    dReal* pdata = nullptr;
    const int numElements = ExtractContiguousArrayToPointer(odata, &pdata);
    if (numElements >= 0) {
        _ptrajectory->Insert(index, pdata, numElements, bOverwrite);
    }
    else {
        std::vector<dReal>& vdata = _vdataCache;
        vdata = ExtractArray<dReal>(odata);
        _ptrajectory->Insert(index,vdata, bOverwrite);
    }
}

void PyTrajectoryBase::Insert(size_t index, object odata, PyConfigurationSpecificationPtr pyspec)
{
    const ConfigurationSpecification spec = openravepy::GetConfigurationSpecification(pyspec);
    dReal* pdata = nullptr;
    const int numElements = ExtractContiguousArrayToPointer(odata, &pdata);
    if (numElements >= 0) {
        _ptrajectory->Insert(index, pdata, numElements, spec);
    }
    else {
        std::vector<dReal>& vdata = _vdataCache;
        vdata = ExtractArray<dReal>(odata);
        _ptrajectory->Insert(index, vdata, spec);
    }
}

void PyTrajectoryBase::Insert(size_t index, object odata, PyConfigurationSpecificationPtr pyspec, bool bOverwrite)
{
    const ConfigurationSpecification spec = openravepy::GetConfigurationSpecification(pyspec);
    dReal* pdata = nullptr;
    const int numElements = ExtractContiguousArrayToPointer(odata, &pdata);
    if (numElements >= 0) {
        _ptrajectory->Insert(index, pdata, numElements, spec, bOverwrite);
    }
    else {
        std::vector<dReal>& vdata = _vdataCache;
        vdata = ExtractArray<dReal>(odata);
        _ptrajectory->Insert(index, vdata, spec, bOverwrite);
    }
}

void PyTrajectoryBase::Insert(size_t index, object odata, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup)
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    this->Insert(index, odata, pyspec);
}

void PyTrajectoryBase::Insert(size_t index, object odata, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup, bool bOverwrite)
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    this->Insert(index, odata, pyspec, bOverwrite);
}

void PyTrajectoryBase::Remove(size_t startindex, size_t endindex)
{
    _ptrajectory->Remove(startindex,endindex);
}

object PyTrajectoryBase::Sample(dReal time) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->Sample(values,time);
    return toPyArray(values);
}

object PyTrajectoryBase::Sample(dReal time, PyConfigurationSpecificationPtr pyspec) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->Sample(values,time,openravepy::GetConfigurationSpecification(pyspec), true);
    return toPyArray(values);
}

object PyTrajectoryBase::Sample(dReal time, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    // pybind11 does not like implicit conversion, so fails to convert this function into
    // object Sample(dReal time, PyConfigurationSpecificationPtr pyspec) const;
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->Sample(time, pyspec);
}

object PyTrajectoryBase::SampleFromPrevious(object odata, dReal time, PyConfigurationSpecificationPtr pyspec) const
{
    std::vector<dReal>& vdata = _vdataCache;
    if (!ExtractContiguousArrayToVector(odata, vdata)) {
        vdata = ExtractArray<dReal>(odata);
    }
    _ptrajectory->Sample(vdata,time,openravepy::GetConfigurationSpecification(pyspec), false);
    return toPyArray(vdata);
}

object PyTrajectoryBase::SampleFromPrevious(object odata, dReal time, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->SampleFromPrevious(odata, time, pyspec);
}

object PyTrajectoryBase::SamplePoints2D(object otimes) const
{
    std::vector<dReal>& vtimes = _vtimesCache;
    if (!ExtractContiguousArrayToVector(otimes, vtimes)) {
        vtimes = ExtractArray<dReal>(otimes);
    }
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->SamplePoints(values,vtimes);

    const int numdof = _ptrajectory->GetConfigurationSpecification().GetDOF();
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    py::array_t<dReal> pypos = toPyArray(values);
    pypos.resize({(int) values.size()/numdof, numdof});
    return pypos;
#else // USE_PYBIND11_PYTHON_BINDINGS
    npy_intp dims[] = { npy_intp(values.size()/numdof), npy_intp(numdof) };
    PyObject *pypos = PyArray_SimpleNew(2,dims, sizeof(dReal)==8 ? PyArray_DOUBLE : PyArray_FLOAT);
    if( !values.empty() ) {
        memcpy(PyArray_DATA(pypos), values.data(), values.size()*sizeof(values[0]));
    }
    return py::to_array_astype<dReal>(pypos);
#endif // USE_PYBIND11_PYTHON_BINDINGS
}

object PyTrajectoryBase::SamplePoints2D(object otimes, PyConfigurationSpecificationPtr pyspec) const
{
    ConfigurationSpecification spec = openravepy::GetConfigurationSpecification(pyspec);
    std::vector<dReal>& vtimes = _vtimesCache;
    if (!ExtractContiguousArrayToVector(otimes, vtimes)) {
        vtimes = ExtractArray<dReal>(otimes);
    }
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->SamplePoints(values, vtimes, spec);

    const int numdof = spec.GetDOF();
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    py::array_t<dReal> pypos = toPyArray(values);
    pypos.resize({(int) values.size()/numdof, numdof});
    return pypos;
#else // USE_PYBIND11_PYTHON_BINDINGS
    npy_intp dims[] = { npy_intp(values.size()/numdof), npy_intp(numdof) };
    PyObject *pypos = PyArray_SimpleNew(2,dims, sizeof(dReal)==8 ? PyArray_DOUBLE : PyArray_FLOAT);
    if( !values.empty() ) {
        memcpy(PyArray_DATA(pypos), values.data(), values.size()*sizeof(values[0]));
    }
    return py::to_array_astype<dReal>(pypos);
#endif // USE_PYBIND11_PYTHON_BINDINGS
}


object _ConvertToObject(const std::vector<dReal>& values, int numdof)
{
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    py::array_t<dReal> pypos = toPyArray(values);
    pypos.resize({(int) values.size()/numdof, numdof});
    return pypos;
#else // USE_PYBIND11_PYTHON_BINDINGS
    npy_intp dims[] = { npy_intp(values.size()/numdof), npy_intp(numdof) };
    PyObject *pypos = PyArray_SimpleNew(2,dims, sizeof(dReal)==8 ? PyArray_DOUBLE : PyArray_FLOAT);
    if( !values.empty() ) {
        memcpy(PyArray_DATA(pypos), values.data(), values.size()*sizeof(values[0]));
    }
    return py::to_array_astype<dReal>(pypos);
#endif // USE_PYBIND11_PYTHON_BINDINGS
}

object PyTrajectoryBase::SamplePointsSameDeltaTime2D(dReal deltatime,
                                                     bool ensureLastPoint) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->SamplePointsSameDeltaTime(values, deltatime, ensureLastPoint);
    const int numdof = _ptrajectory->GetConfigurationSpecification().GetDOF();
    return _ConvertToObject(values, numdof);
}


object PyTrajectoryBase::SamplePointsSameDeltaTime2D(dReal deltatime,
                                                     bool ensureLastPoint,
                                                     PyConfigurationSpecificationPtr pyspec) const
{
    std::vector<dReal>& values = _vdataCache;
    ConfigurationSpecification spec = openravepy::GetConfigurationSpecification(pyspec);
    _ptrajectory->SamplePointsSameDeltaTime(values, deltatime, ensureLastPoint, spec);

    const int numdof = spec.GetDOF();
    return _ConvertToObject(values, numdof);
}

object PyTrajectoryBase::SamplePointsSameDeltaTime2D(dReal deltatime,
                                                     bool ensureLastPoint,
                                                     OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->SamplePointsSameDeltaTime2D(deltatime, ensureLastPoint, pyspec);
}

object PyTrajectoryBase::SampleRangeSameDeltaTime2D(dReal deltatime,
                                                    dReal startTime,
                                                    dReal stopTime,
                                                    bool ensureLastPoint) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->SampleRangeSameDeltaTime(values, deltatime, startTime, stopTime, ensureLastPoint);
    const int numdof = _ptrajectory->GetConfigurationSpecification().GetDOF();
    return _ConvertToObject(values, numdof);
}


object PyTrajectoryBase::SampleRangeSameDeltaTime2D(dReal deltatime,
                                                    dReal startTime,
                                                    dReal stopTime,
                                                    bool ensureLastPoint,
                                                    PyConfigurationSpecificationPtr pyspec) const
{
    std::vector<dReal>& values = _vdataCache;
    ConfigurationSpecification spec = openravepy::GetConfigurationSpecification(pyspec);
    _ptrajectory->SampleRangeSameDeltaTime(values, deltatime, startTime, stopTime, ensureLastPoint, spec);

    const int numdof = spec.GetDOF();
    return _ConvertToObject(values, numdof);
}

object PyTrajectoryBase::SampleRangeSameDeltaTime2D(dReal deltatime,
                                                    dReal startTime,
                                                    dReal stopTime,
                                                    bool ensureLastPoint,
                                                    OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->SampleRangeSameDeltaTime2D(deltatime, startTime, stopTime, ensureLastPoint, pyspec);
}

object PyTrajectoryBase::SamplePoints2D(object otimes, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->SamplePoints2D(otimes, pyspec);
}

object PyTrajectoryBase::GetConfigurationSpecification() const {
    return py::to_object(openravepy::toPyConfigurationSpecification(_ptrajectory->GetConfigurationSpecification()));
}

size_t PyTrajectoryBase::GetNumWaypoints() const {
    return _ptrajectory->GetNumWaypoints();
}

object PyTrajectoryBase::GetWaypoints(size_t startindex, size_t endindex) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->GetWaypoints(startindex,endindex,values);
    return toPyArray(values);
}

object PyTrajectoryBase::GetWaypoints(size_t startindex, size_t endindex, PyConfigurationSpecificationPtr pyspec) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->GetWaypoints(startindex,endindex,values,openravepy::GetConfigurationSpecification(pyspec));
    return toPyArray(values);
}

object PyTrajectoryBase::GetWaypoints(size_t startindex, size_t endindex, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->GetWaypoints(startindex, endindex, pyspec);
}

// similar to GetWaypoints except returns a 2D array, one row for every waypoint
object PyTrajectoryBase::GetWaypoints2D(size_t startindex, size_t endindex) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->GetWaypoints(startindex,endindex,values);
    const int numdof = _ptrajectory->GetConfigurationSpecification().GetDOF();

#ifdef USE_PYBIND11_PYTHON_BINDINGS
    py::array_t<dReal> pypos = toPyArray(values);
    pypos.resize({(int) values.size()/numdof, numdof});
    return pypos;
#else // USE_PYBIND11_PYTHON_BINDINGS
    npy_intp dims[] = { npy_intp(values.size()/numdof), npy_intp(numdof) };
    PyObject *pypos = PyArray_SimpleNew(2,dims, sizeof(dReal)==8 ? PyArray_DOUBLE : PyArray_FLOAT);
    if( !values.empty() ) {
        memcpy(PyArray_DATA(pypos), values.data(), values.size()*sizeof(values[0]));
    }
    return py::to_array_astype<dReal>(pypos);
#endif // USE_PYBIND11_PYTHON_BINDINGS
}

object PyTrajectoryBase::__getitem__(int index) const
{
    return GetWaypoint(index);
}

object PyTrajectoryBase::__getitem__(py::slice indices) const
{
    std::vector<int>vindices;
    int len = _ptrajectory->GetNumWaypoints();
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    // https://github.com/pybind/pybind11/issues/1095
    int start, stop, step;
    size_t len_ = len, start_, stop_, step_, slicelength_;
    if (indices.compute(len_, &start_, &stop_, &step_, &slicelength_)) {
        step = step_;
        start = start_;
        stop = stop_;
    }
    else {
        step = 1;
        start = step > 0 ? 0 : len-1;
        stop = step > 0 ? len : -1;
    }
#else
    int step = !IS_PYTHONOBJECT_NONE(indices.step()) ? extract<int>(indices.step()) : 1;
    int start = !IS_PYTHONOBJECT_NONE(indices.start()) ? extract<int>(indices.start()) : step>0 ? 0 : len-1;
    int stop = !IS_PYTHONOBJECT_NONE(indices.stop()) ? extract<int>(indices.stop()) : step>0 ? len : -1;
#endif // USE_PYBIND11_PYTHON_BINDINGS
    if(step==0) {
        throw OPENRAVE_EXCEPTION_FORMAT0(_("step cannot be 0"),ORE_InvalidArguments);
    }
    for(int i=start; step>0 ? i<stop : i>stop; i+=step) {
        vindices.push_back(i);
    }

    std::vector<dReal>& values = _vdataCache; // workspace
    _ptrajectory->GetWaypoint(0,values);
    const int nindices = vindices.size();
    const int numdof = _ptrajectory->GetConfigurationSpecification().GetDOF();
    const int nvalues = values.size();
    OPENRAVE_ASSERT_OP(numdof, ==, nvalues);
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    py::array_t<dReal> pypos({nindices, numdof});
    py::buffer_info buf = pypos.request();
    dReal* ppos = (dReal*) buf.ptr;
#else // USE_PYBIND11_PYTHON_BINDINGS
    npy_intp dims[] = { npy_intp(nindices), npy_intp(numdof) };
    PyObject *pypos = PyArray_SimpleNew(2,dims, sizeof(dReal)==8 ? PyArray_DOUBLE : PyArray_FLOAT);
#endif // USE_PYBIND11_PYTHON_BINDINGS
    const int waypointSize = nvalues * sizeof(values[0]);
    for(int i = 0; i < nindices; i++) {
        _ptrajectory->GetWaypoint(vindices[i],values);
#ifdef USE_PYBIND11_PYTHON_BINDINGS
        memcpy(ppos, values.data(), waypointSize);
        ppos += numdof;
#else
        memcpy(PyArray_BYTES(pypos)+(i*waypointSize), values.data(), waypointSize);
#endif // USE_PYBIND11_PYTHON_BINDINGS
    }
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    return pypos;
#else // USE_PYBIND11_PYTHON_BINDINGS
    return py::to_array_astype<dReal>(pypos);
#endif // USE_PYBIND11_PYTHON_BINDINGS
}

object PyTrajectoryBase::GetAllWaypoints2D() const
{
    return GetWaypoints2D(0, _ptrajectory->GetNumWaypoints());
}

object PyTrajectoryBase::GetWaypoints2D(size_t startindex, size_t endindex, PyConfigurationSpecificationPtr pyspec) const
{
    std::vector<dReal>& values = _vdataCache;
    ConfigurationSpecification spec = openravepy::GetConfigurationSpecification(pyspec);
    _ptrajectory->GetWaypoints(startindex,endindex,values,spec);
    const int numdof = spec.GetDOF();
    const int nvalues = values.size();
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    py::array_t<dReal> pypos = toPyArray(values);
    pypos.resize({nvalues/numdof, numdof});
    return pypos;
#else // USE_PYBIND11_PYTHON_BINDINGS
    npy_intp dims[] = { npy_intp(nvalues/numdof), npy_intp(numdof) };
    PyObject *pypos = PyArray_SimpleNew(2,dims, sizeof(dReal)==8 ? PyArray_DOUBLE : PyArray_FLOAT);
    if( !values.empty() ) {
        memcpy(PyArray_DATA(pypos), values.data(), nvalues*sizeof(values[0]));
    }
    return py::to_array_astype<dReal>(pypos);
#endif // USE_PYBIND11_PYTHON_BINDINGS
}

object PyTrajectoryBase::GetWaypoints2D(size_t startindex, size_t endindex, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->GetWaypoints2D(startindex, endindex, pyspec);
}

object PyTrajectoryBase::GetAllWaypoints2D(PyConfigurationSpecificationPtr pyspec) const
{
    return GetWaypoints2D(0, _ptrajectory->GetNumWaypoints(), pyspec);
}

object PyTrajectoryBase::GetAllWaypoints2D(OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->GetAllWaypoints2D(pyspec);
}

object PyTrajectoryBase::GetWaypoint(int index) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->GetWaypoint(index,values);
    return toPyArray(values);
}

object PyTrajectoryBase::GetWaypoint(int index, PyConfigurationSpecificationPtr pyspec) const
{
    std::vector<dReal>& values = _vdataCache;
    _ptrajectory->GetWaypoint(index,values,openravepy::GetConfigurationSpecification(pyspec));
    return toPyArray(values);
}

object PyTrajectoryBase::GetWaypoint(int index, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group> pygroup) const
{
    PyConfigurationSpecificationPtr pyspec(new PyConfigurationSpecification(*pygroup));
    return this->GetWaypoint(index, pyspec);
}

size_t PyTrajectoryBase::GetFirstWaypointIndexAfterTime(dReal time) const
{
    return _ptrajectory->GetFirstWaypointIndexAfterTime(time);
}

dReal PyTrajectoryBase::GetDuration() const {
    return _ptrajectory->GetDuration();
}

void PyTrajectoryBase::deserialize(const string& s)
{
    std::stringstream ss(s);
    _ptrajectory->deserialize(ss);
}

object PyTrajectoryBase::serialize(object options)
{
    std::stringstream ss;
    ss << std::setprecision(std::numeric_limits<dReal>::digits10+1);
    _ptrajectory->serialize(ss,pyGetIntFromPy(options, 0));
    return py::to_object(ss.str());
}

void PyTrajectoryBase::SaveToFile(const std::string& filename, object options)
{
    std::ofstream f(filename.c_str(), ios::binary);
    f << std::setprecision(std::numeric_limits<dReal>::digits10+1);
    _ptrajectory->serialize(f, pyGetIntFromPy(options, 0));
    f.close(); // necessary?
}

void PyTrajectoryBase::LoadFromFile(const std::string& filename)
{
    std::ifstream f(filename.c_str(), ios::binary);
    _ptrajectory->deserialize(f);
    f.close(); // necessary?
}

TrajectoryBasePtr PyTrajectoryBase::GetTrajectory() {
    return _ptrajectory;
}

TrajectoryBasePtr GetTrajectory(object o)
{
    extract_<PyTrajectoryBasePtr> pytrajectory(o);
    if( pytrajectory.check() ) {
        return GetTrajectory((PyTrajectoryBasePtr)pytrajectory);
    }
    return TrajectoryBasePtr();
}

TrajectoryBasePtr GetTrajectory(PyTrajectoryBasePtr pytrajectory)
{
    return !pytrajectory ? TrajectoryBasePtr() : pytrajectory->GetTrajectory();
}

PyInterfaceBasePtr toPyTrajectory(TrajectoryBasePtr ptrajectory, PyEnvironmentBasePtr pyenv)
{
    return !ptrajectory ? PyInterfaceBasePtr() : PyInterfaceBasePtr(new PyTrajectoryBase(ptrajectory,pyenv));
}

object toPyTrajectory(TrajectoryBasePtr ptraj, object opyenv)
{
    extract_<PyEnvironmentBasePtr> pyenv(opyenv);
    if( pyenv.check() ) {
        return py::to_object(toPyTrajectory(ptraj,(PyEnvironmentBasePtr)pyenv));
    }
    return py::none_();
}

PyEnvironmentBasePtr toPyEnvironment(PyTrajectoryBasePtr pytraj)
{
    return pytraj->GetEnv();
}

PyTrajectoryBasePtr RaveCreateTrajectory(PyEnvironmentBasePtr pyenv, const std::string& name)
{
    TrajectoryBasePtr p = OpenRAVE::RaveCreateTrajectory(GetEnvironment(pyenv), name);
    if( !p ) {
        return PyTrajectoryBasePtr();
    }
    return PyTrajectoryBasePtr(new PyTrajectoryBase(p,pyenv));
}

#ifndef USE_PYBIND11_PYTHON_BINDINGS
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(serialize_overloads, serialize, 0, 1)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(SaveToFile_overloads, SaveToFile, 1, 2)
#endif //

#ifdef USE_PYBIND11_PYTHON_BINDINGS
void init_openravepy_trajectory(py::module& m)
#else
void init_openravepy_trajectory()
#endif
{
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    using namespace py::literals; // "..."_a
#endif
    void (PyTrajectoryBase::*Init1)(PyConfigurationSpecificationPtr) = &PyTrajectoryBase::Init;
    void (PyTrajectoryBase::*Init2)(OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) = &PyTrajectoryBase::Init;
    void (PyTrajectoryBase::*Insert1)(size_t,object) = &PyTrajectoryBase::Insert;
    void (PyTrajectoryBase::*Insert2)(size_t,object,bool) = &PyTrajectoryBase::Insert;
    void (PyTrajectoryBase::*Insert3)(size_t,object,PyConfigurationSpecificationPtr) = &PyTrajectoryBase::Insert;
    void (PyTrajectoryBase::*Insert4)(size_t,object,PyConfigurationSpecificationPtr,bool) = &PyTrajectoryBase::Insert;
    void (PyTrajectoryBase::*Insert5)(size_t, object, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) = &PyTrajectoryBase::Insert;
    void (PyTrajectoryBase::*Insert6)(size_t, object, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>, bool) = &PyTrajectoryBase::Insert;
    object (PyTrajectoryBase::*Sample1)(dReal) const = &PyTrajectoryBase::Sample;
    object (PyTrajectoryBase::*Sample2)(dReal, PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::Sample;
    object (PyTrajectoryBase::*Sample3)(dReal, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::Sample;
    object (PyTrajectoryBase::*SampleFromPrevious1)(object, dReal, PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::SampleFromPrevious;
    object (PyTrajectoryBase::*SampleFromPrevious2)(object, dReal, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::SampleFromPrevious;
    object (PyTrajectoryBase::*SamplePoints2D1)(object) const = &PyTrajectoryBase::SamplePoints2D;
    object (PyTrajectoryBase::*SamplePoints2D2)(object, PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::SamplePoints2D;
    object (PyTrajectoryBase::*SamplePoints2D3)(object, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::SamplePoints2D;
    object (PyTrajectoryBase::*SamplePointsSameDeltaTime2D1)(dReal, bool) const = &PyTrajectoryBase::SamplePointsSameDeltaTime2D;
    object (PyTrajectoryBase::*SamplePointsSameDeltaTime2D2)(dReal, bool, PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::SamplePointsSameDeltaTime2D;
    object (PyTrajectoryBase::*SamplePointsSameDeltaTime2D3)(dReal, bool, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::SamplePointsSameDeltaTime2D;
    object (PyTrajectoryBase::*SampleRangeSameDeltaTime2D1)(dReal, dReal, dReal, bool) const = &PyTrajectoryBase::SampleRangeSameDeltaTime2D;
    object (PyTrajectoryBase::*SampleRangeSameDeltaTime2D2)(dReal, dReal, dReal, bool, PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::SampleRangeSameDeltaTime2D;
    object (PyTrajectoryBase::*SampleRangeSameDeltaTime2D3)(dReal, dReal, dReal, bool, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::SampleRangeSameDeltaTime2D;
    object (PyTrajectoryBase::*GetWaypoints1)(size_t,size_t) const = &PyTrajectoryBase::GetWaypoints;
    object (PyTrajectoryBase::*GetWaypoints2)(size_t,size_t,PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::GetWaypoints;
    object (PyTrajectoryBase::*GetWaypoints3)(size_t, size_t, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::GetWaypoints;
    object (PyTrajectoryBase::*GetWaypoints2D1)(size_t,size_t) const = &PyTrajectoryBase::GetWaypoints2D;
    object (PyTrajectoryBase::*GetWaypoints2D2)(size_t,size_t,PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::GetWaypoints2D;
    object (PyTrajectoryBase::*GetWaypoints2D3)(size_t, size_t, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::GetWaypoints2D;
    object (PyTrajectoryBase::*GetAllWaypoints2D1)() const = &PyTrajectoryBase::GetAllWaypoints2D;
    object (PyTrajectoryBase::*GetAllWaypoints2D2)(PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::GetAllWaypoints2D;
    object (PyTrajectoryBase::*GetAllWaypoints2D3)(OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::GetAllWaypoints2D;
    object (PyTrajectoryBase::*GetWaypoint1)(int) const = &PyTrajectoryBase::GetWaypoint;
    object (PyTrajectoryBase::*GetWaypoint2)(int,PyConfigurationSpecificationPtr) const = &PyTrajectoryBase::GetWaypoint;
    object (PyTrajectoryBase::*GetWaypoint3)(int, OPENRAVE_SHARED_PTR<ConfigurationSpecification::Group>) const = &PyTrajectoryBase::GetWaypoint;
    object (PyTrajectoryBase::*__getitem__1)(int) const = &PyTrajectoryBase::__getitem__;
    object (PyTrajectoryBase::*__getitem__2)(slice) const = &PyTrajectoryBase::__getitem__;

#ifdef USE_PYBIND11_PYTHON_BINDINGS
    class_<PyTrajectoryBase, OPENRAVE_SHARED_PTR<PyTrajectoryBase>, PyInterfaceBase>(m, "Trajectory", DOXY_CLASS(TrajectoryBase))
#else
    class_<PyTrajectoryBase, OPENRAVE_SHARED_PTR<PyTrajectoryBase>, bases<PyInterfaceBase> >("Trajectory", DOXY_CLASS(TrajectoryBase), no_init)
#endif
    .def("Init", Init1, PY_ARGS("spec") DOXY_FN(TrajectoryBase,Init))
    .def("Init", Init2, PY_ARGS("group") DOXY_FN(TrajectoryBase,Init))
    .def("Insert",Insert1, PY_ARGS("index","data") DOXY_FN(TrajectoryBase,Insert "size_t; const std::vector; bool"))
    .def("Insert",Insert2, PY_ARGS("index","data","overwrite") DOXY_FN(TrajectoryBase,Insert "size_t; const std::vector; bool"))
    .def("Insert",Insert3, PY_ARGS("index","data","spec") DOXY_FN(TrajectoryBase,Insert "size_t; const std::vector; const ConfigurationSpecification"))
    .def("Insert",Insert4, PY_ARGS("index","data","spec","overwrite") DOXY_FN(TrajectoryBase,Insert "size_t; const std::vector; const ConfigurationSpecification; bool"))
    .def("Insert",Insert5, PY_ARGS("index","data","group") DOXY_FN(TrajectoryBase,Insert "size_t; const std::vector; const ConfigurationSpecification::Group"))
    .def("Insert",Insert6, PY_ARGS("index","data","group","overwrite") DOXY_FN(TrajectoryBase,Insert "size_t; const std::vector; const ConfigurationSpecification::Group; bool"))
    .def("Remove",&PyTrajectoryBase::Remove, PY_ARGS("startindex","endindex") DOXY_FN(TrajectoryBase,Remove))
    .def("Sample",Sample1, PY_ARGS("time") DOXY_FN(TrajectoryBase,Sample "std::vector; dReal"))
    .def("Sample",Sample2, PY_ARGS("time","spec") DOXY_FN(TrajectoryBase,Sample "std::vector; dReal; const ConfigurationSpecification"))
    .def("Sample",Sample3, PY_ARGS("time","group") DOXY_FN(TrajectoryBase,Sample "std::vector; dReal; const ConfigurationSpecification::Group"))
    .def("SampleFromPrevious", SampleFromPrevious1, PY_ARGS("data","time","spec") DOXY_FN(TrajectoryBase,Sample "std::vector; dReal; const ConfigurationSpecification"))
    .def("SampleFromPrevious", SampleFromPrevious2, PY_ARGS("data","time","group") DOXY_FN(TrajectoryBase,Sample "std::vector; dReal; const ConfigurationSpecification::Group"))
    .def("SamplePoints2D",SamplePoints2D1, PY_ARGS("times") DOXY_FN(TrajectoryBase,SamplePoints2D "std::vector; std::vector"))
    .def("SamplePoints2D",SamplePoints2D2, PY_ARGS("times","spec") DOXY_FN(TrajectoryBase,SamplePoints2D "std::vector; std::vector; const ConfigurationSpecification"))
    .def("SamplePoints2D",SamplePoints2D3, PY_ARGS("times","group") DOXY_FN(TrajectoryBase,SamplePoints2D "std::vector; std::vector; const ConfigurationSpecification::Group"))
    .def("SamplePointsSameDeltaTime2D",SamplePointsSameDeltaTime2D1, PY_ARGS("deltatime","ensurelastpoint") DOXY_FN(TrajectoryBase,SamplePointsSameDeltaTime2D "dReal; bool"))
    .def("SamplePointsSameDeltaTime2D",SamplePointsSameDeltaTime2D2, PY_ARGS("deltatime","ensurelastpoint","spec") DOXY_FN(TrajectoryBase,SamplePointsSameDeltaTime2D "dReal; bool; const ConfigurationSpecification"))
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    // why boost::python can resolve this overload?
    .def("SamplePointsSameDeltaTime2D",SamplePointsSameDeltaTime2D3, PY_ARGS("deltatime","ensurelastpoint","group") DOXY_FN(TrajectoryBase,SamplePointsSameDeltaTime2D "dReal; bool; const ConfigurationSpecification::Group"))
#endif
    .def("SampleRangeSameDeltaTime2D",SampleRangeSameDeltaTime2D1, PY_ARGS("deltatime","startTime","stopTime","ensurelastpoint") DOXY_FN(TrajectoryBase,SampleRangeSameDeltaTime2D "dReal; dReal; dReal; bool"))
    .def("SampleRangeSameDeltaTime2D",SampleRangeSameDeltaTime2D2, PY_ARGS("deltatime","startTime","stopTime","ensurelastpoint","spec") DOXY_FN(TrajectoryBase,SampleRangeSameDeltaTime2D "dReal; dReal; dReal; bool; const ConfigurationSpecification"))
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    // why boost::python can resolve this overload?
    .def("SampleRangeSameDeltaTime2D",SampleRangeSameDeltaTime2D3, PY_ARGS("deltatime","startTime","stopTime","ensurelastpoint","group") DOXY_FN(TrajectoryBase,SampleRangeSameDeltaTime2D "dReal; dReal; dReal; bool; const ConfigurationSpecification::Group"))
#endif
    .def("GetConfigurationSpecification",&PyTrajectoryBase::GetConfigurationSpecification,DOXY_FN(TrajectoryBase,GetConfigurationSpecification))
    .def("GetNumWaypoints",&PyTrajectoryBase::GetNumWaypoints,DOXY_FN(TrajectoryBase,GetNumWaypoints))
    .def("GetWaypoints",GetWaypoints1, PY_ARGS("startindex","endindex") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector"))
    .def("GetWaypoints",GetWaypoints2, PY_ARGS("startindex","endindex","spec") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector, const ConfigurationSpecification&"))
    .def("GetWaypoints",GetWaypoints3, PY_ARGS("startindex","endindex","group") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector, const ConfigurationSpecification&::Group"))
    .def("GetWaypoints2D",GetWaypoints2D1, PY_ARGS("startindex","endindex") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector"))
    .def("GetWaypoints2D",GetWaypoints2D2, PY_ARGS("startindex","endindex","spec") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector, const ConfigurationSpecification&"))
    .def("GetWaypoints2D",GetWaypoints2D3, PY_ARGS("startindex","endindex","group") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector, const ConfigurationSpecification::Group&"))
    .def("GetAllWaypoints2D",GetAllWaypoints2D1,DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector"))
    .def("GetAllWaypoints2D",GetAllWaypoints2D2, PY_ARGS("spec") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector, const ConfigurationSpecification&"))
    .def("GetAllWaypoints2D",GetAllWaypoints2D3, PY_ARGS("group") DOXY_FN(TrajectoryBase, GetWaypoints "size_t; size_t; std::vector, const ConfigurationSpecification::Group&"))
    .def("GetWaypoint",GetWaypoint1, PY_ARGS("index") DOXY_FN(TrajectoryBase, GetWaypoint "int; std::vector"))
    .def("GetWaypoint",GetWaypoint2, PY_ARGS("index","spec") DOXY_FN(TrajectoryBase, GetWaypoint "int; std::vector; const ConfigurationSpecification"))
    .def("GetWaypoint",GetWaypoint3, PY_ARGS("index","group") DOXY_FN(TrajectoryBase, GetWaypoint "int; std::vector; const ConfigurationSpecification::Group"))
    .def("GetFirstWaypointIndexAfterTime",&PyTrajectoryBase::GetFirstWaypointIndexAfterTime, DOXY_FN(TrajectoryBase, GetFirstWaypointIndexAfterTime))
    .def("GetDuration",&PyTrajectoryBase::GetDuration,DOXY_FN(TrajectoryBase, GetDuration))
#ifdef USE_PYBIND11_PYTHON_BINDINGS
    .def("serialize", &PyTrajectoryBase::serialize,
         "options"_a = py::none_(),
         DOXY_FN(TrajectoryBase, serialize)
         )
    .def("SaveToFile", &PyTrajectoryBase::SaveToFile,
         "filename"_a,
         "options"_a = py::none_(),
         DOXY_FN(TrajectoryBase, SaveToFile)
         )
#else
    .def("serialize",&PyTrajectoryBase::serialize,serialize_overloads(PY_ARGS("options") DOXY_FN(TrajectoryBase,serialize)))
    .def("SaveToFile",&PyTrajectoryBase::SaveToFile,SaveToFile_overloads(PY_ARGS("filename, options") DOXY_FN(TrajectoryBase,SaveToFile)))
#endif
    .def("deserialize",&PyTrajectoryBase::deserialize, PY_ARGS("data") DOXY_FN(TrajectoryBase,deserialize))
    .def("LoadFromFile",&PyTrajectoryBase::LoadFromFile, PY_ARGS("filename") DOXY_FN(TrajectoryBase,deserialize))
    .def("__len__",&PyTrajectoryBase::GetNumWaypoints,DOXY_FN(TrajectoryBase,__len__))
    .def("__getitem__",__getitem__1, PY_ARGS("index") DOXY_FN(TrajectoryBase, __getitem__ "int"))
    .def("__getitem__",__getitem__2, PY_ARGS("indices") DOXY_FN(TrajectoryBase, __getitem__ "slice"))
    ;

#ifdef USE_PYBIND11_PYTHON_BINDINGS
    m.def("RaveCreateTrajectory",openravepy::RaveCreateTrajectory, PY_ARGS("env","name") DOXY_FN1(RaveCreateTrajectory));
#else
    def("RaveCreateTrajectory",openravepy::RaveCreateTrajectory, PY_ARGS("env","name") DOXY_FN1(RaveCreateTrajectory));
#endif
}

}
