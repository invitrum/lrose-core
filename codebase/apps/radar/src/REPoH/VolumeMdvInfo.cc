/**
 * @file VolumeMdvInfo.cc
 */

//------------------------------------------------------------------
#include "VolumeMdvInfo.hh"
#include "RepohParams.hh"
#include "GridFieldsAll.hh"
#include <FiltAlgVirtVol/NamePair.hh>
#include <FiltAlgVirtVol/UrlSpec.hh>
#include <Mdv/MdvxField.hh>
#include <toolsa/LogStream.hh>
#include <cmath>

//------------------------------------------------------------------
VolumeMdvInfo::VolumeMdvInfo(void)
{
}

//------------------------------------------------------------------
VolumeMdvInfo::~VolumeMdvInfo(void)
{
}

//------------------------------------------------------------------
std::vector<double> VolumeMdvInfo::noisePerRange(double noiseAt100Km) const
{
  vector<double> noiseDbz;
  double range = _coord.minx;
  for (int i=0; i<_coord.nx; ++i, range += _coord.dx)
  {
    noiseDbz.push_back(noiseAt100Km + 20.0*(log10(range) - log10(100.0)));
  }
  return noiseDbz;
}

//------------------------------------------------------------------
bool VolumeMdvInfo::initialInitializeInput(const time_t &t, const UrlSpec &u,
					   const RepohParams &parms)
{
  vector<NamePair> fields = u.fieldNames();
  if (fields.empty())
  {
    return false;
  }

  LOG(DEBUG) << " creating initial input state using " << u._url
	     << " field " << fields[0]._external;
  DsMdvx mdv;
  mdv.setReadTime(Mdvx::READ_FIRST_BEFORE, u._url, 0, t);
  mdv.addReadField(fields[0]._external);
  if (parms.restrict_vertical_levels)
  {
    mdv.setReadVlevelLimits(parms._vertical_level_range[0],
			    parms._vertical_level_range[1]);
  }

  LOG(DEBUG) << "Reading";
  if (mdv.readVolume())
  {
    LOG(ERROR)<< "reading volume " << u._url;
    return false;
  }
  MdvxField *f = mdv.getFieldByName(fields[0]._external);
  if (f == NULL)
  {
    LOG(ERROR) << "reading field " << fields[0]._external << " from " <<
      u._url;
    return  false;
  }
  Mdvx::field_header_t hdr = f->getFieldHeader();
  _nz = hdr.nz;
  _nx = hdr.nx;
  _ny = hdr.ny;
  _masterHdr = mdv.getMasterHeader();
  _fieldHdr = hdr;
  _vlevelHdr = f->getVlevelHeader();
  _proj = MdvxProj(_masterHdr, hdr);
  _coord = _proj.getCoord();
  MdvxRadar mdvRadar;
  if (mdvRadar.loadFromMdvx(mdv) && mdvRadar.radarParamsAvail())
  {
    _hasWavelength = true;
    _hasAltitude = true;
    DsRadarParams &rparams = mdvRadar.getRadarParams();
    _wavelength = rparams.wavelength;
    _altitude = rparams.altitude;
  }
  else
  {
    _hasWavelength = false;
    _hasAltitude = true;
    _altitude = _masterHdr.sensor_alt;
  }

  _vlevel.clear();
  for (int i=0; i<_nz; ++i)
  {
    _vlevel.push_back(_vlevelHdr.level[i]);
  }
  return true;
}

bool VolumeMdvInfo::initializeInput(const time_t &t, const UrlSpec &u,
				    const RepohParams &parms,
				    GridFieldsAll &data)
{
  vector<NamePair> fields = u.fieldNames();
  vector<NamePair> values = u.valueNames();

  LOG(DEBUG) << " creating input for " << u._url;
  if (!fields.empty())
  {
    DsMdvx mdv;
    mdv.setReadTime(Mdvx::READ_FIRST_BEFORE, u._url, 0, t);
    for (size_t i=0; i<fields.size(); ++i)
    {
      LOG(DEBUG) << " data = " << fields[i]._external;
      mdv.addReadField(fields[i]._external);
    }
    if (parms.restrict_vertical_levels)
    {
      mdv.setReadVlevelLimits(parms._vertical_level_range[0],
			      parms._vertical_level_range[1]);
    }
    LOG(DEBUG) << "Reading";
    if (mdv.readVolume())
    {
      LOG(ERROR)<< "reading volume " << u._url;
      return false;
    }

    for (size_t i=0; i<fields.size(); ++i)
    {
      LOG(DEBUG) << "Storing locally " << fields[i]._external;
      MdvxField *f = mdv.getFieldByName(fields[i]._external);
      if (f == NULL)
      {
	LOG(ERROR) << "reading field " << fields[i]._external << " from " <<
	  u._url;
	return  false;
      }
      f->convertType(Mdvx::ENCODING_FLOAT32, Mdvx::COMPRESSION_NONE);
      Mdvx::field_header_t hdr = f->getFieldHeader();
      if (_nz != hdr.nz || _nx != hdr.nx || _ny != hdr.ny)
      {
	LOG(ERROR) << "Unequal dimensions";
	return false;
      }
      for (int j=0; j<hdr.nz; ++j)
      {
	Grid2d g(fields[i]._internal, hdr.nx, hdr.ny, hdr.missing_data_value);
	GriddedData gd(g);
	fl32 *fdata = (fl32 *)f->getVol();
	int k0 = hdr.nx*hdr.ny*j;
	for (int k=0; k<hdr.nx*hdr.ny ; ++k)
	{
	  if (fdata[k0+k] != hdr.bad_data_value &&
	      fdata[k0+k] != hdr.missing_data_value)
	  {
	    gd.setValue(k, static_cast<double>(fdata[k0+k]));
	  }
	}
	data.addField(gd, j);
      }
    }
  }

  if (!values.empty())
  {
    // if (values.size() > 1)
    // {
    LOG(ERROR) << "No values have been implemented";
    return false;
    // }
    // SoundingR *s = new SoundingR(values[0]._external, u._url, t);
    // _soundings.push_back(s);
  }
  return true;
}

void VolumeMdvInfo::outputToUrl(const time_t &t, const UrlSpec &u,
				const GridFieldsAll &data)
{
  vector<NamePair> names = u.fieldNames();
  // construct each named field

  LOG(DEBUG) << "Writing " << names.size() << " fields to " << u._url;
  DsMdvx out;
  
  _masterHdr.time_gen = _masterHdr.time_end = _masterHdr.time_centroid = t;
  _masterHdr.time_expire = t;
  _masterHdr.forecast_time = t;
  _masterHdr.forecast_delta = 0;
  out.setMasterHeader(_masterHdr);
  for (size_t i=0; i<names.size(); ++i)
  {
    _outputFieldToUrl(names[i]._internal, names[i]._external, t, data, out);
  }
  out.setWriteLdataInfo();
  if (out.writeToDir(u._url))
  {
    LOG(ERROR) << "Unable to write mdv";
  }
}

void VolumeMdvInfo::_outputFieldToUrl(const std::string &internalName,
				      const std::string &externalName,
				      const time_t &t, 
				      const GridFieldsAll &data,
				      DsMdvx &out)
{
  _fieldHdr.encoding_type = Mdvx::ENCODING_FLOAT32;
  _fieldHdr.data_element_nbytes = 4;
  _fieldHdr.volume_size = _nx*_ny*_nz*_fieldHdr.data_element_nbytes;
  _fieldHdr.compression_type = Mdvx::COMPRESSION_NONE; // done later
  _fieldHdr.transform_type = Mdvx::DATA_TRANSFORM_NONE;
  _fieldHdr.scaling_type = Mdvx::SCALING_DYNAMIC;
  _fieldHdr.scale = 1.0;
  _fieldHdr.bias = 0.0;
  _fieldHdr.forecast_delta = 0;
  _fieldHdr.forecast_time = t;
  strncpy(_fieldHdr.field_name_long, externalName.c_str(),
	  MDV_LONG_FIELD_LEN-1);
  _fieldHdr.field_name_long[MDV_LONG_FIELD_LEN-1] = '\0';
  strncpy(_fieldHdr.field_name, externalName.c_str(), MDV_SHORT_FIELD_LEN-1);
  _fieldHdr.field_name[MDV_SHORT_FIELD_LEN-1] = '\0';
  strncpy(_fieldHdr.units, "units", MDV_UNITS_LEN-1);
  _fieldHdr.units[MDV_UNITS_LEN-1] = '\0';

  // Try to get some sample data so as to set the missing values
  double missingV;
  if (!data.sampleMissingValue(internalName, missingV))
  {
    LOG(ERROR) << "Missing data " << internalName;
    LOG(WARNING) << "Write some code to fill missing";
    return;
  }
  
  _fieldHdr.bad_data_value = missingV;
  _fieldHdr.missing_data_value = missingV;

  // finally, add the field
  MdvxField *f = new MdvxField(_fieldHdr, _vlevelHdr, (void *)0,true,false);
  fl32 *fo = (fl32 *)f->getVol();

  // Loop through again and populate the new field
  data.retrieveVolumeData(internalName, fo);
  out.addField(f);
}


