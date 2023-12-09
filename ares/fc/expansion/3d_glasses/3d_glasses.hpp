struct F3D_Glasses : Expansion {
  F3D_Glasses(Node::Port);
  auto write(n8 data) -> void override;

private:
  n1 lens;
};
