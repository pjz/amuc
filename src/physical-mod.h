struct PhmBuffer {
  int ampl_scale;
  ShortBuffer *var_data[colors_max],  // modified at runtime
              const_data[colors_max]; // initial values
  bool init(const int ampl);
  void reset();
  bool set_phys_model(int col,ShortBuffer *pmdat);
};

extern PhmBuffer phm_buf;
